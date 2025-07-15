#include <chrono>
#include <ctime>
#include <iostream>
#include <nlohmann/json.hpp>
#include "helpers/url.h"
#include "helpers/ip_address.h"
#include "helpers/socket_address.h"
#include "helpers/thread.h"
#include "app.h"

namespace SocialNetwork {

static void set_options_(socket_t sock)
{
    httplib::detail::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
#ifdef SO_REUSEPORT
    httplib::detail::set_socket_opt(sock, SOL_SOCKET, SO_REUSEPORT, 1);
#endif
}

static std::string time_local_str_()
{
    auto p = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(p);

    std::stringstream ss;
    //                                       08/Apr/2025:12:06:54 +0000
    ss << std::put_time(std::localtime(&t), "%d/%b/%Y:%H:%M:%S %z");
    return ss.str();
}

static std::string time_gmt_str_()
{
    auto p = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(p);
    std::tm gmt_tm;
    gmtime_r(&t, &gmt_tm);

    std::stringstream ss;
    //                                       Sat, 01 Jan 2005 11:00:00 GMT
    ss << std::put_time(std::localtime(&t), "%a, %d %b %Y %H:%M:%S GMT");
    return ss.str();
}

//-----------------------------------------------------------------------------



App::~App()
{
    if (http_server_) http_server_->stop();
    if (http_server_thread_.joinable()) {
        http_server_thread_.join();
    }
}

App::App(std::shared_ptr<cxxopts::ParseResult> cli_opts)
:   logger_(Logging::configure_logger({ {"type", "stdout"}, {"color", "true"}, {"level", "5"} })),
    conf_(std::make_shared<Configuration>(logger_, cli_opts))
{}

void App::run()
{
    LOG_INFOR(std::format("running..."));

    try {
        conf_->show_configuration();

        db_start();

        on_liveness_check([this]()->bool {
            // liveness probe (работоспособность).
            // собираем условие работоспособности сервиса.
            // как минимум, если сокеты внутренних серверов переведены
            // в состояние LISTENING
            return true;
        });
        on_readiness_check([]()->bool {
            // readiness probe (готовность).
            // XXX: можно будет сюда засунуть некую логику, чтобы понимать
            //      готово приложение к труду или нет. а пока что будем считать,
            //      что если endpoint доступен и отвечает, значит - ok
            return true;
        });
        http_start();

        for (;;) {
            sleep(1);
        }
    }
    catch (std::exception& ex) {
        LOG_ERROR(std::format("App::run() exception: {}",
            ex.what()));
    }
}

void App::db_start()
{
    static bool db_client_started = false;

    if (db_client_started) return;

    try {
        ConnectionPool::ConnectionStrCollection masters;
        ConnectionPool::ConnectionStrCollection replicas;
        // https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING

        {
            UrlHelpers::Url url(conf_->config().pgsql_master.url);
            std::string tag = std::format("{}:{}",
                url.get_host(),
                url.get_port());
            db_host_tags.insert(tag);
            std::string conn_str = std::format("user={} password={} host={} port={} dbname={} connect_timeout=60 application_name=social_network",
                conf_->config().pgsql_master.login,
                conf_->config().pgsql_master.password,
                url.get_host(),
                url.get_port(),
                url.get_path().substr(1));
            masters.push_back(std::make_pair(conn_str, tag));
        }
        for (const auto& replica : conf_->config().pgsql_replica) {
            UrlHelpers::Url url(replica.url);
            std::string tag = std::format("{}:{}",
                url.get_host(),
                url.get_port());
            db_host_tags.insert(tag);
            std::string conn_str = std::format("user={} password={} host={} port={} dbname={} connect_timeout=60 application_name=social_network",
                replica.login,
                replica.password,
                url.get_host(),
                url.get_port(),
                url.get_path().substr(1));
            replicas.push_back(std::make_pair(conn_str, tag));
        }

        db_pool_ = std::make_shared<ConnectionPool>(masters, replicas, conf_->config().http_threads_count);
        if (db_pool_) {
            db_client_started = true;
        }
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("db_start exception: {}", ex.what()));
    }
}

void App::http_start()
{
    static const std::string http_server_thread_name("HttpSrv");

    if (http_server_
    &&  http_server_->is_running()) return;

    try {
        // регистрируем сервер для Prometheus-метрик
        exposer_ = std::make_unique<prometheus::Exposer>(conf_->config().prometheus_listening);
        metrics_ = std::make_shared<Metrics>(db_host_tags);
        exposer_->RegisterCollectable(metrics_->registry());

        // регистрируем HTTP-сервер
        http_server_ = std::make_unique<httplib::Server>();
        if (!http_server_->is_valid()) throw std::runtime_error("server has an error...");

        NetHelpers::SocketAddress sock_addr(conf_->config().http_listening);
        http_server_->bind_to_port(sock_addr.host().to_string(), sock_addr.port());

        LOG_INFOR(std::format("{} socket was configured into listening state: {}",
            http_server_thread_name, sock_addr.to_string()));

        // устанавливаем наш ThreadPool для обработки очереди запросов
        http_server_->new_task_queue = [this] {
            return new ThreadPoolAdaptor(http_server_thread_name + std::string("Pool"),
                                         logger_,
                                         conf_->config().http_threads_count,
                                         conf_->config().http_queue_capacity);
        };

                    // Keep-Alive connection
        http_server_->set_keep_alive_max_count(2)
                    .set_keep_alive_timeout(10)
                    // Timeouts
                    .set_read_timeout(5, 0)
                    .set_write_timeout(5, 0)
                    .set_idle_interval(0, 100'000/*usec*/)
                    // включаем SO_REUSEADDR и SO_REUSEPORT
                    .set_socket_options(set_options_)
                    // лимит размера тела запроса, защита от DDoS: 1 MB
                    .set_payload_max_length(1 * 1024 * 1024)
                    // обработчик ошибок
                    .set_error_handler([this](const auto& req, auto& res) { error_handler(req, res); })
                    // обработчик исключений
                    .set_exception_handler([this](const auto& req, auto& res, std::exception_ptr ep) { exception_handler(req, res, ep); })
                    // предварительная обработка (после приема запроса)
                    .set_pre_routing_handler([this](const auto& req, auto& res) { return (pre_routing_handler(req, res)) ? (httplib::Server::HandlerResponse::Unhandled) : (httplib::Server::HandlerResponse::Handled); })
                    // окончательная обработка (перед отправкой ответа)
                    .set_post_routing_handler([this](const auto& req, auto& res) { post_routing_handler(req, res); })
                    // логирование запросов
                    .set_logger([this](const auto& req, const auto& res) { log_handler(req, res); })
                    // обработчики
                    .Get("/livez",  [this](const auto& req, auto& res) { liveness_handler(req, res); })
                    .Get("/readyz", [this](const auto& req, auto& res) { readiness_handler(req, res); });

        // создаем сервисы
        service_cache    = std::make_shared<CacheService>(CACHE_CAPACITY, std::chrono::seconds(CACHE_TTL_SEC));
        service_database = std::make_shared<DatabaseService>(logger_, metrics_, db_pool_);
        service_auth     = std::make_shared<AuthService>(logger_, metrics_, service_database);
        service_user     = std::make_unique<UserService>(logger_, metrics_, service_database);
        service_friend   = std::make_unique<FriendService>(logger_, metrics_, service_database, service_cache, service_auth);
        service_post     = std::make_unique<PostService>(logger_, metrics_, service_database, service_cache, service_auth);
        service_dialog   = std::make_unique<DialogService>(logger_, metrics_, service_database, service_auth);

        // регистрируем сервисы в HTTP сервере
        service_user->register_endpoints(http_server_.get());
        service_friend->register_endpoints(http_server_.get());
        service_post->register_endpoints(http_server_.get());
        service_dialog->register_endpoints(http_server_.get());

        http_server_thread_ = std::thread([this]()->void {
            ThreadHelpers::block_signals();
            http_server_->listen_after_bind();
        });
        ThreadHelpers::set_name(http_server_thread_.native_handle(), http_server_thread_name);
    }
    catch (std::exception& ex) {
        LOG_ERROR(std::format("{} exception: {}", http_server_thread_name, ex.what()));
    }
}

bool App::pre_routing_handler(const httplib::Request& req, httplib::Response& res)
{
    if ((req.path == "/livez" || req.path == "/readyz")
    ||  (service_user   && service_user->pre_routing_validation(req))
    ||  (service_friend && service_friend->pre_routing_validation(req))
    ||  (service_post   && service_post->pre_routing_validation(req))
    ||  (service_dialog && service_dialog->pre_routing_validation(req))) {
        return true;
    }
    res.status = httplib::StatusCode::NotImplemented_501;
    return false;
}

void App::liveness_handler(const httplib::Request& /*req*/, httplib::Response& res)
{
    constexpr auto result_html = "{}\n";
    constexpr auto ok          = "ok";
    constexpr auto fail        = "fail";

    if (liveness_check_cb_
    &&  liveness_check_cb_()) {
        res.set_content(std::format(result_html, ok), "text/plain");
    } else {
        res.set_content(std::format(result_html, fail), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
}

void App::readiness_handler(const httplib::Request& /*req*/, httplib::Response& res)
{
    constexpr auto result_html = "{}\n";
    constexpr auto ok          = "ok";
    constexpr auto fail        = "fail";

    if (readiness_check_cb_
    &&  readiness_check_cb_()) {
        res.set_content(std::format(result_html, ok), "text/plain");
    } else {
        res.set_content(std::format(result_html, fail), "text/plain");
        res.status = httplib::StatusCode::InternalServerError_500;
    }
}

void App::post_routing_handler(const httplib::Request& /*req*/, httplib::Response& res)
{
    static const std::string srv_name(std::string("social_network/1.0")
                                    + std::format(" (Linux) httplib/{}", CPPHTTPLIB_VERSION));

    res.set_header("Date", time_gmt_str_());
    res.set_header("Server", srv_name);
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("Content-Security-Policy", "default-src 'self'");
}

void App::error_handler(const httplib::Request& req, httplib::Response& res)
{
    constexpr auto error_html =
R"(<html><head><title>{} {}</title></head>
<body><h1>Can't handle '{} {}'</h1></body></html>
)";
    auto body = std::format(error_html, res.status, httplib::status_message(res.status), req.method, req.path);
    res.set_content(body, "text/html");
}

void App::exception_handler(const httplib::Request& /*req*/, httplib::Response& res, std::exception_ptr ep)
{
    constexpr auto exception_html = R"(<h1>Error 500</h1><p>{}</p>)";
    std::string mes;
    try {
        std::rethrow_exception(ep);
    }
    catch (std::exception &e) {
        mes.assign(e.what());
    }
    catch (...) { 
        mes.assign("Unknown Exception");
    }
    auto body = std::format(exception_html, res.status);
    res.set_content(body, "text/html");
    res.status = httplib::StatusCode::InternalServerError_500;
}

void App::log_handler(const httplib::Request& req, const httplib::Response& res)
{
    auto request         = std::format("{} {} {}", req.method, req.path, req.version);
    auto body_bytes_sent = res.get_header_value("Content-Length");
    auto http_user_agent = req.get_header_value("User-Agent", "-");

    // NOTE: From NGINX default access log format
    // log_format combined '$remote_addr - $remote_user [$time_local] '
    //                     '"$request" $status $body_bytes_sent '
    //                     '"$http_referer" "$http_user_agent"';

    constexpr auto log_html = R"({} - {} [{}] "{}" {} {} "{}" "{}")";
    // 127.0.0.1 - - [08/Apr/2025:12:07:01 +0000] "GET /livez HTTP/1.1" 200 3 "-" "curl/8.12.1"
    LOG_TRACE(std::format(log_html, req.remote_addr,
                                    /*remote_user=*/"-",
                                    time_local_str_(),
                                    request,
                                    res.status,
                                    body_bytes_sent,
                                    /*http_referer=*/"-",
                                    http_user_agent));
}

} // namespace SocialNetwork
