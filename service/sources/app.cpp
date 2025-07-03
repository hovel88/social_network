#include <chrono>
#include <ctime>
#include <iostream>
#include <regex>
#include <nlohmann/json.hpp>
#include <bcrypt/BCrypt.hpp>
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

static bool is_valid_uuid_(const std::string& id) {
    static const std::regex uuid_regex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"
    );
    return std::regex_match(id, uuid_regex);
}

//-----------------------------------------------------------------------------



App::~App()
{
    if (http_server_) http_server_->stop();
    if (http_server_thread_.joinable()) {
        http_server_thread_.join();
    }
    if (db_client_thread_.joinable()) {
        db_client_thread_.join();
    }
}

App::App(std::shared_ptr<cxxopts::ParseResult> cli_opts)
:   logger_(Logging::configure_logger({ {"type", "stdout"}, {"color", "true"}, {"level", "5"} })),
    conf_(std::make_shared<Configuration>(logger_, cli_opts))
{
    try {
        const auto& cli = *cli_opts;

        const std::string drop_key("index_drop");
        if (cli.count(drop_key)) {
            indexes_to_drop_ = cli[drop_key].as<std::vector<std::string>>();
        }
        const std::string create_key("index_add");
        if (cli.count(create_key)) {
            indexes_to_create_ = cli[create_key].as<std::vector<std::string>>();
        }
    }
    catch (...) {}
}

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
    static const std::string db_client_thread_name("SqlClient");
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

            db_client_thread_ = std::thread([this]()->void {
                ThreadHelpers::block_signals();
                // db_create_users_table();
                // for (const auto& one : indexes_to_drop_) {
                //     if (one == "names_search") {
                //         db_drop_index_users_names_search();
                //     }
                // }
                // for (const auto& one : indexes_to_create_) {
                //     if (one == "names_search") {
                //         db_create_index_users_names_search();
                //     }
                // }
            });
            ThreadHelpers::set_name(db_client_thread_.native_handle(), db_client_thread_name);
        }
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("{} exception: {}", db_client_thread_name, ex.what()));
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
                    .Post("/login", [this](const auto& req, auto& res) {
                        auto start = std::chrono::steady_clock::now();
                        bool ok    = login_handler(req, res);
                        auto end   = std::chrono::steady_clock::now();
                        metrics_->count_request_login();
                        if (!ok) metrics_->count_failed_request_login();
                        if (ok)  metrics_->store_latency_request_login(std::chrono::duration<double>(end - start).count());
                    })
                    .Post("/user/register", [this](const auto& req, auto& res) {
                        auto start = std::chrono::steady_clock::now();
                        bool ok    = user_register_handler(req, res);
                        auto end   = std::chrono::steady_clock::now();
                        metrics_->count_request_user_register();
                        if (!ok) metrics_->count_failed_request_user_register();
                        if (ok)  metrics_->store_latency_request_user_register(std::chrono::duration<double>(end - start).count());
                    })
                    .Get("/user/get/:id", [this](const auto& req, auto& res) {
                        auto start = std::chrono::steady_clock::now();
                        bool ok    = user_get_id_handler(req, res);
                        auto end   = std::chrono::steady_clock::now();
                        metrics_->count_request_user_get_id();
                        if (!ok) metrics_->count_failed_request_user_get_id();
                        if (ok)  metrics_->store_latency_request_user_get_id(std::chrono::duration<double>(end - start).count());
                    })
                    .Get("/user/search", [this](const auto& req, auto& res) {
                        auto start = std::chrono::steady_clock::now();
                        bool ok    = user_search_handler(req, res);
                        auto end   = std::chrono::steady_clock::now();
                        metrics_->count_request_user_search();
                        if (!ok) metrics_->count_failed_request_user_search();
                        if (ok)  metrics_->store_latency_request_user_search(std::chrono::duration<double>(end - start).count());
                    })
                    .Get("/livez",  [this](const auto& req, auto& res) { liveness_handler(req, res); })
                    .Get("/readyz", [this](const auto& req, auto& res) { readiness_handler(req, res); });

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
    if (req.path == "/livez"
    ||  req.path == "/readyz"
    ||  req.path == "/login"
    ||  req.path == "/user/register"
    ||  req.path.starts_with("/user/get/")
    ||  req.path == "/user/search") {
        return true;
    }
    res.status = httplib::StatusCode::NotImplemented_501;
    return false;
}

bool App::login_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("id")
    ||  !json.contains("password")) {
        LOG_ERROR(std::format("login_handler: request params does not contain 'id' and/or 'password'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["id"].is_string()
    ||  !json["password"].is_string()) {
        LOG_ERROR(std::format("login_handler: request params 'id' and 'password' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!is_valid_uuid_(json["id"].get<std::string>())) {
        LOG_ERROR(std::format("login_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_pool_) {
        LOG_ERROR(std::format("login_handler: there is no connection to DB"));

        response = {{"code", 503}, {"message", "Server Error: login_handler: there is no connection to DB"}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    static const std::string query =
        "SELECT id, pwd_hash "
        "  FROM users "
        " WHERE id = $1";

    bool ok = false;
    try {
        const std::string id{json["id"].get<std::string>()};
        const std::string pwd{json["password"].get<std::string>()};

        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        LOG_TRACE(std::format("login_handler: query to {} #{} tag='{}'",
            (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

        pqxx::work tx(*scoped_conn.conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{id});
        if (result.empty()) {
            // пользователь не найден
            res.status = httplib::StatusCode::NotFound_404;
        } else {
            for (const auto& row : result) {
                const auto& [row_id, row_pwd_hash] = row.as<std::string, std::string>();
                if (!BCrypt::validatePassword(pwd, row_pwd_hash)) {
                    // неверный пароль
                    LOG_ERROR(std::format("login_handler: request param 'password' is not match"));
                    res.status = httplib::StatusCode::BadRequest_400;
                    return false;
                }
                // успешная аутентификация
                response = {{"token", row_id}};
                break;
            }
            ok = true;
        }
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));

        response = {{"code", 500}, {"message", std::format("Error SQL: {}", ex.what())}};
        res.status = httplib::StatusCode::InternalServerError_500;
    }

    res.set_content(response.dump(), "application/json");
    return ok;
}

bool App::user_register_handler(const httplib::Request& req, httplib::Response& res)
{
    auto json = nlohmann::json::parse(req.body);
    nlohmann::json response{};

    if (!json.contains("password")
    ||  !json.contains("first_name")
    ||  !json.contains("second_name")
    ||  !json.contains("birthdate")
    ||  !json.contains("biography")
    ||  !json.contains("city")) {
        LOG_ERROR(std::format("login_handler: request params does not contain 'password', 'first_name', 'second_name', 'birthdate', 'biography' and/or 'city'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!json["password"].is_string()
    ||  !json["first_name"].is_string()
    ||  !json["second_name"].is_string()
    ||  !json["birthdate"].is_string()
    ||  !json["biography"].is_string()
    ||  !json["city"].is_string()) {
        LOG_ERROR(std::format("user_register_handler: request params 'password', 'first_name', 'second_name', 'birthdate', 'biography' and 'city' should be a string"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (json["password"].get<std::string>().length() < 8) {
        // минимальная длина 8 символов
        LOG_ERROR(std::format("user_register_handler: request param 'password' should contain at least 8 characters"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    {
        std::tm t{};
        std::istringstream ss(json["birthdate"].get<std::string>());
        ss >> std::get_time(&t, "%Y-%m-%d"); // 2017-02-01
        if (ss.fail() || t.tm_year < 0 || t.tm_year > 107) {
            // 18лет (с 2007 г.д.), 2007 - 1900 = 107
            LOG_ERROR(std::format("user_register_handler: request param 'birthdate' is invalid"));
            res.status = httplib::StatusCode::BadRequest_400;
            return false;
        }
    }

    if (!db_pool_) {
        LOG_ERROR(std::format("user_register_handler: there is no connection to DB"));

        response = {{"code", 503}, {"message", "Server Error: user_register_handler: there is no connection to DB"}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    static const std::string query =
        "INSERT INTO users (first_name, second_name, birthdate, biography, city, pwd_hash) "
        "     VALUES ($1, $2, $3, $4, $5, $6) "
        "  RETURNING id";

    bool ok = false;
    try {
        const std::string pwd{json["password"].get<std::string>()};
        const std::string fname{json["first_name"].get<std::string>()};
        const std::string sname{json["second_name"].get<std::string>()};
        const std::string bdate{json["birthdate"].get<std::string>()};
        const std::string bio{json["biography"].get<std::string>()};
        const std::string city{json["city"].get<std::string>()};
        std::string hashed_pwd = BCrypt::generateHash(pwd, 12);

        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        LOG_TRACE(std::format("user_register_handler: query to {} #{} tag='{}'",
            (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

        pqxx::work tx(*scoped_conn.conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{fname, sname, bdate, bio, city, hashed_pwd});
        tx.commit();
        if (result.empty()) {
            response = {{"code", 500}, {"message", std::format("Can't register user '{} {}'", fname, sname)}};
            res.status = httplib::StatusCode::InternalServerError_500;
        } else {
            for (const auto& row : result) {
                const auto& [row_id] = row.as<std::string>();
                // успешная регистрация
                response = {{"user_id", row_id}};
                break;
            }
            ok = true;
        }
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));

        response = {{"code", 500}, {"message", std::format("Error SQL: {}", ex.what())}};
        res.status = httplib::StatusCode::InternalServerError_500;
    }

    res.set_content(response.dump(), "application/json");
    return ok;
}

bool App::user_get_id_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response{};

    if (!req.path_params.contains("id")) {
        LOG_ERROR(std::format("user_get_id_handler: request params does not contain 'id'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!is_valid_uuid_(req.path_params.at("id"))) {
        LOG_ERROR(std::format("user_get_id_handler: request param 'id' is not an UUID format"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_pool_) {
        LOG_ERROR(std::format("user_get_id_handler: there is no connection to DB"));

        response = {{"code", 503}, {"message", "Server Error: user_get_id_handler: there is no connection to DB"}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    static const std::string query =
        "SELECT first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE id = $1";

    bool ok = false;
    try {
        const std::string id{req.path_params.at("id")};

        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        LOG_TRACE(std::format("user_get_id_handler: query to {} #{} tag='{}'",
            (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

        pqxx::work tx(*scoped_conn.conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{id});
        if (result.empty()) {
            // анкета не найдена
            res.status = httplib::StatusCode::NotFound_404;
        } else {
            for (const auto& row : result) {
                const auto& [row_fname, row_sname, row_bdate, row_bio, row_city] = row.as<std::string, std::string, std::string, std::string, std::string>();
                // успешное получение анкеты пользователя
                response = {{"id", id},
                            {"first_name", row_fname},
                            {"second_name", row_sname},
                            {"birthdate", row_bdate},
                            {"biography", row_bio},
                            {"city", row_city}};
                break;
            }
            ok = true;
        }
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));

        response = {{"code", 500}, {"message", std::format("Error SQL: {}", ex.what())}};
        res.status = httplib::StatusCode::InternalServerError_500;
    }

    res.set_content(response.dump(), "application/json");
    return ok;
}

bool App::user_search_handler(const httplib::Request& req, httplib::Response& res)
{
    nlohmann::json response = nlohmann::json::array({});

    if (!req.has_param("first_name")
    ||  !req.has_param("last_name")) {
        LOG_ERROR(std::format("user_search_handler: request params does not contain 'first_name' and/or 'last_name'"));
        res.status = httplib::StatusCode::BadRequest_400;
        return false;
    }

    if (!db_pool_) {
        LOG_ERROR(std::format("user_search_handler: there is no connection to DB"));

        response = {{"code", 503}, {"message", "Server Error: user_search_handler: there is no connection to DB"}};
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
        return false;
    }

    static const std::string query =
        "SELECT id, first_name, second_name, birthdate, biography, city "
        "  FROM users "
        " WHERE first_name LIKE $1 AND second_name LIKE $2 "
        " ORDER BY id "
        " LIMIT 100";

    bool ok = false;
    try {
        const std::string first_name{req.get_param_value("first_name") + "%"};
        const std::string second_name{req.get_param_value("last_name") + "%"};

        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::REPLICA);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        LOG_TRACE(std::format("user_search_handler: query to {} #{} tag='{}'",
            (scoped_conn.node_type == ConnectionPool::NodeType::MASTER ? "MASTER" : "REPLICA"), scoped_conn.node_num, scoped_conn.node_tag));

        pqxx::work tx(*scoped_conn.conn.get());
        pqxx::result result = tx.exec(query, pqxx::params{first_name, second_name});

        for (const auto& row : result) {
            const auto& [row_id, row_fname, row_sname, row_bdate, row_bio, row_city] = row.as<std::string, std::string, std::string, std::string, std::string, std::string>();
            // собираем массив
            response.push_back({{"id", row_id},
                                {"first_name", row_fname},
                                {"second_name", row_sname},
                                {"birthdate", row_bdate},
                                {"biography", row_bio},
                                {"city", row_city}});
        }
        ok = true;
    } catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));

        response = {{"code", 500}, {"message", std::format("Error SQL: {}", ex.what())}};
        res.status = httplib::StatusCode::InternalServerError_500;
    }

    res.set_content(response.dump(), "application/json");
    return ok;
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

void App::db_create_users_table()
{
    static const std::string query =
        "CREATE TABLE IF NOT EXISTS users ("
        "  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),"
        "  created_at  TIMESTAMP    NOT NULL DEFAULT NOW(),"
        "  pwd_hash    VARCHAR(100) NOT NULL,"
        "  first_name  VARCHAR(50)  NOT NULL,"
        "  second_name VARCHAR(50)  NOT NULL,"
        "  birthdate   DATE,"
        "  biography   TEXT,"
        "  city        VARCHAR(50)"
        ")";

    LOG_DEBUG(std::format("table 'users', trying to create table if not exists"));

    try {
        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        pqxx::work tx(*scoped_conn.conn.get());
        tx.exec(query).no_rows();
        tx.commit();
    }
    catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));
    }
}

void App::db_create_index_users_names_search()
{
    // можно использовать GIN + trigram для полнотекстовых поисков
    // (включая LIKE '%ан%'), при этом надо включить расширение.
    // однако GIN работает медленнее B-tree и занимает больше места,
    // хотя и позволяет не только префиксный поиск, но и с любыми
    // LIKE-запросами (префикс, суффикс, вхождение подстроки).
    // XXX: почему то просто так планировщик не захотел использовать
    //      именно этот индекс. вместо него предпочел Parallel Seq Scan
    //      для запросов (first_name LIKE '%ва%' AND second_name LIKE '%ан%').
    //      вероятно дело в отсутствии статистики у планировщика, или просто
    //      очень низкая селективность и планировщик отбросил этот индекс.
    // static const std::string query0 =
    //     "CREATE EXTENSION IF NOT EXISTS pg_trgm";
    // static const std::string query1 =
    //     "CREATE INDEX IF NOT EXISTS users_names_gin_idx "
    //     "ON users USING GIN(first_name gin_trgm_ops, second_name gin_trgm_ops)";

    // для B-tree надо использовать text_pattern_ops, т.к. по умолчанию он
    // не поддерживает поиск по префиксу (LIKE 'Ив%') для типов text/varchar.
    // а для поисков по суффиксу и вхождения подстроки - не подходит вообще!
    static const std::string query2 =
        "CREATE INDEX IF NOT EXISTS users_names_btree_idx "
        "ON users(first_name text_pattern_ops, second_name text_pattern_ops)";

    LOG_DEBUG(std::format("table 'users', trying to create index: users_names_btree_idx"));

    std::string query{};
    try {
        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        pqxx::work tx(*scoped_conn.conn.get());
        // query = query0;
        // tx.exec(query).no_rows();
        // query = query1;
        // tx.exec(query).no_rows();
        query = query2;
        tx.exec(query).no_rows();
        tx.commit();
    }
    catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));
    }
}

void App::db_drop_index_users_names_search()
{
    // static const std::string query1 =
    //     "DROP INDEX IF EXISTS users_names_gin_idx";
    static const std::string query2 =
        "DROP INDEX IF EXISTS users_names_btree_idx";

    LOG_DEBUG(std::format("table 'users', trying to drop index: users_names_btree_idx"));

    std::string query{};
    try {
        ScopedConnection scoped_conn(db_pool_, ConnectionPool::NodeType::MASTER);
        metrics_->count_request_to_host(scoped_conn.node_tag);
        pqxx::work tx(*scoped_conn.conn.get());
        // query = query1;
        // tx.exec(query).no_rows();
        query = query2;
        tx.exec(query).no_rows();
        tx.commit();
    }
    catch (std::exception& ex) {
        LOG_ERROR(std::format("SQL connection exception: {} (query: {})", ex.what(), query));
    }
}

} // namespace SocialNetwork
