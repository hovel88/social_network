#include <chrono>
#include <ctime>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <nlohmann/json.hpp>
#include "helpers/url.h"
#include "helpers/ip_address.h"
#include "helpers/socket_address.h"
#include "helpers/thread.h"
#include "app.h"
#include "http_middleware_auth.h"
#include "configuration.h"

static inline const char* status_message(drogon::HttpStatusCode status) {
    switch (status) {
    case drogon::HttpStatusCode::k100Continue:                      return "Continue";
    case drogon::HttpStatusCode::k101SwitchingProtocols:            return "Switching Protocol";
    case drogon::HttpStatusCode::k102Processing:                    return "Processing";
    case drogon::HttpStatusCode::k103EarlyHints:                    return "Early Hints";
    case drogon::HttpStatusCode::k200OK:                            return "OK";
    case drogon::HttpStatusCode::k201Created:                       return "Created";
    case drogon::HttpStatusCode::k202Accepted:                      return "Accepted";
    case drogon::HttpStatusCode::k203NonAuthoritativeInformation:   return "Non-Authoritative Information";
    case drogon::HttpStatusCode::k204NoContent:                     return "No Content";
    case drogon::HttpStatusCode::k205ResetContent:                  return "Reset Content";
    case drogon::HttpStatusCode::k206PartialContent:                return "Partial Content";
    case drogon::HttpStatusCode::k207MultiStatus:                   return "Multi-Status";
    case drogon::HttpStatusCode::k208AlreadyReported:               return "Already Reported";
    case drogon::HttpStatusCode::k226IMUsed:                        return "IM Used";
    case drogon::HttpStatusCode::k300MultipleChoices:               return "Multiple Choices";
    case drogon::HttpStatusCode::k301MovedPermanently:              return "Moved Permanently";
    case drogon::HttpStatusCode::k302Found:                         return "Found";
    case drogon::HttpStatusCode::k303SeeOther:                      return "See Other";
    case drogon::HttpStatusCode::k304NotModified:                   return "Not Modified";
    case drogon::HttpStatusCode::k305UseProxy:                      return "Use Proxy";
    case drogon::HttpStatusCode::k306Unused:                        return "unused";
    case drogon::HttpStatusCode::k307TemporaryRedirect:             return "Temporary Redirect";
    case drogon::HttpStatusCode::k308PermanentRedirect:             return "Permanent Redirect";
    case drogon::HttpStatusCode::k400BadRequest:                    return "Bad Request";
    case drogon::HttpStatusCode::k401Unauthorized:                  return "Unauthorized";
    case drogon::HttpStatusCode::k402PaymentRequired:               return "Payment Required";
    case drogon::HttpStatusCode::k403Forbidden:                     return "Forbidden";
    case drogon::HttpStatusCode::k404NotFound:                      return "Not Found";
    case drogon::HttpStatusCode::k405MethodNotAllowed:              return "Method Not Allowed";
    case drogon::HttpStatusCode::k406NotAcceptable:                 return "Not Acceptable";
    case drogon::HttpStatusCode::k407ProxyAuthenticationRequired:   return "Proxy Authentication Required";
    case drogon::HttpStatusCode::k408RequestTimeout:                return "Request Timeout";
    case drogon::HttpStatusCode::k409Conflict:                      return "Conflict";
    case drogon::HttpStatusCode::k410Gone:                          return "Gone";
    case drogon::HttpStatusCode::k411LengthRequired:                return "Length Required";
    case drogon::HttpStatusCode::k412PreconditionFailed:            return "Precondition Failed";
    case drogon::HttpStatusCode::k413RequestEntityTooLarge:         return "Payload Too Large";
    case drogon::HttpStatusCode::k414RequestURITooLarge:            return "URI Too Long";
    case drogon::HttpStatusCode::k415UnsupportedMediaType:          return "Unsupported Media Type";
    case drogon::HttpStatusCode::k416RequestedRangeNotSatisfiable:  return "Range Not Satisfiable";
    case drogon::HttpStatusCode::k417ExpectationFailed:             return "Expectation Failed";
    case drogon::HttpStatusCode::k418ImATeapot:                     return "I'm a teapot";
    case drogon::HttpStatusCode::k421MisdirectedRequest:            return "Misdirected Request";
    case drogon::HttpStatusCode::k422UnprocessableEntity:           return "Unprocessable Content";
    case drogon::HttpStatusCode::k423Locked:                        return "Locked";
    case drogon::HttpStatusCode::k424FailedDependency:              return "Failed Dependency";
    case drogon::HttpStatusCode::k425TooEarly:                      return "Too Early";
    case drogon::HttpStatusCode::k426UpgradeRequired:               return "Upgrade Required";
    case drogon::HttpStatusCode::k428PreconditionRequired:          return "Precondition Required";
    case drogon::HttpStatusCode::k429TooManyRequests:               return "Too Many Requests";
    case drogon::HttpStatusCode::k431RequestHeaderFieldsTooLarge:   return "Request Header Fields Too Large";
    case drogon::HttpStatusCode::k451UnavailableForLegalReasons:    return "Unavailable For Legal Reasons";
    case drogon::HttpStatusCode::k500InternalServerError:           return "Internal Server Error";
    case drogon::HttpStatusCode::k501NotImplemented:                return "Not Implemented";
    case drogon::HttpStatusCode::k502BadGateway:                    return "Bad Gateway";
    case drogon::HttpStatusCode::k503ServiceUnavailable:            return "Service Unavailable";
    case drogon::HttpStatusCode::k504GatewayTimeout:                return "Gateway Timeout";
    case drogon::HttpStatusCode::k505HTTPVersionNotSupported:       return "HTTP Version Not Supported";
    case drogon::HttpStatusCode::k506VariantAlsoNegotiates:         return "Variant Also Negotiates";
    case drogon::HttpStatusCode::k507InsufficientStorage:           return "Insufficient Storage";
    case drogon::HttpStatusCode::k508LoopDetected:                  return "Loop Detected";
    case drogon::HttpStatusCode::k510NotExtended:                   return "Not Extended";
    case drogon::HttpStatusCode::k511NetworkAuthenticationRequired: return "Network Authentication Required";

    default: return "Unknown";
    }
}

//-----------------------------------------------------------------------------



App::~App()
{
    if (http_server_) http_server_->quit();
    if (http_server_thread_.joinable()) {
        http_server_thread_.join();
    }
}

App::App()
:   logger_(Configuration::instance().get_logger()),
    http_server_(std::shared_ptr<drogon::HttpAppFramework>(&drogon::app(), [](drogon::HttpAppFramework*){})) // добавили deleter (пустой), т.к. за жизненный цикл объекта отвечает синглтон, а не shared_ptr
{}

void App::run()
{
    LOGGER_INFOR(std::format("running..."));

    try {
        db_start();

        {
            const Configuration& configuration = Configuration::instance();
            configuration.show_configuration();

            // регистрируем сервер для Prometheus-метрик
            exposer_ = std::make_unique<prometheus::Exposer>(configuration.prometheus_listening);
            metrics_ = std::make_shared<Metrics>(db_host_tags);
            exposer_->RegisterCollectable(metrics_->registry());

            UrlHelpers::Url url(configuration.grpc_url);
            grpc_channel_ = grpc::CreateChannel(std::format("{}:{}", url.get_host(), url.get_port()), grpc::InsecureChannelCredentials()); // без SSL/TLS
        }

        kafka_producer = std::make_shared<KafkaProducer>(logger_);

        // создаем сервисы
        service_cache_       = std::make_shared<CacheService>(CACHE_CAPACITY, std::chrono::seconds(CACHE_TTL_SEC));
        service_database_    = std::make_shared<DatabaseService>(logger_, metrics_, db_pool_);
        service_auth_        = std::make_shared<AuthService>(logger_, metrics_, service_database_);
        service_user_http_   = std::make_unique<HttpUserService>(logger_, metrics_, service_database_);
        service_friend_http_ = std::make_unique<HttpFriendService>(logger_, metrics_, service_database_, service_cache_);
        service_post_http_   = std::make_unique<HttpPostService>(logger_, metrics_, service_database_, service_cache_, kafka_producer);
        service_dialog_http_ = std::make_unique<HttpDialogService>(logger_, metrics_, grpc_channel_);

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
        LOGGER_ERROR(std::format("App::run() exception: {}",
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

        const Configuration& configuration = Configuration::instance();

        {
            UrlHelpers::Url url(configuration.pgsql_master.url);
            std::string tag = std::format("{}:{}",
                url.get_host(),
                url.get_port());
            db_host_tags.insert(tag);
            std::string conn_str = std::format("user={} password={} host={} port={} dbname={} connect_timeout=60 application_name=social_network",
                configuration.pgsql_master.login,
                configuration.pgsql_master.password,
                url.get_host(),
                url.get_port(),
                url.get_path().substr(1));
            masters.push_back(std::make_pair(conn_str, tag));
        }
        for (const auto& replica : configuration.pgsql_replica) {
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

        db_pool_ = std::make_shared<ConnectionPool>(masters, replicas, configuration.http_threads_count);
        if (db_pool_) {
            db_client_started = true;
        }
    } catch (std::exception& ex) {
        LOGGER_ERROR(std::format("db_start exception: {}", ex.what()));
    }
}

void App::http_start()
{
    static const std::string http_server_thread_name("HttpSrv");

    if (http_server_
    &&  http_server_->isRunning()) return;

    try {
        const Configuration& configuration = Configuration::instance();

        // регистрируем сервисы в HTTP сервере
        service_user_http_->register_endpoints(http_server_.get());
        service_friend_http_->register_endpoints(http_server_.get());
        service_post_http_->register_endpoints(http_server_.get());
        service_dialog_http_->register_endpoints(http_server_.get());

        // регистрируем и настраиваем HTTP-сервер
        http_server_->setBeforeListenSockOptCallback([this](int sock) {
            const int enable = 1;
            if (setsockopt(sock, IPPROTO_TCP, TCP_FASTOPEN, &enable, sizeof(enable)) == -1) {
                LOGGER_INFOR("setsockopt TCP_FASTOPEN failed");
            }
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
                LOGGER_INFOR("setsockopt SO_REUSEADDR failed");
            }
            #ifdef SO_REUSEPORT
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) == -1) {
                LOGGER_INFOR("setsockopt SO_REUSEPORT failed");
            }
            #endif
        })
        .setAfterAcceptSockOptCallback([](int /*sock*/) {})
        // регистрируем обработчик странички с ответом об ошибке
        .setCustomErrorHandler([this](drogon::HttpStatusCode code, const auto& req)-> drogon::HttpResponsePtr {
            constexpr auto error_html = R"(<html><head><title>{} {}</title></head><body><h1>Can't handle '{} {}'</h1></body></html>)";
            auto body = std::format(error_html, static_cast<int>(code), status_message(code), req->getMethodString(), req->getPath());
            auto res  = drogon::HttpResponse::newHttpResponse(code, drogon::ContentType::CT_TEXT_HTML);
            res->setBody(body);
            return res;
        })
        // регистрируем обработчик странички с ответом об исключениях
        .setExceptionHandler([this](const std::exception& ex, const auto& req, auto&& callback) {
            constexpr auto exception_html = R"(<html><head><title>Exception</title></head><body><h1>Can't handle '{} {}'</h1><p>{}</p></body></html>)";
            auto body = std::format(exception_html, req->getMethodString(), req->getPath(), ex.what());
            auto res  = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k500InternalServerError, drogon::ContentType::CT_TEXT_HTML);
            res->setBody(body);
            callback(res);
        })
        .registerBeginningAdvice([this]() {
            auto listeners = http_server_->getListeners();
            for (const auto& listener : listeners) {
                LOGGER_INFOR(std::format("{} socket was configured into listening state: {}",
                   http_server_thread_name, listener.toIpPort()));
            }
        })
        .registerPreSendingAdvice([this](const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& res) -> void {
            std::stringstream ss;
            auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            //                                       08/Apr/2025:12:06:54 +0000
            ss << std::put_time(std::localtime(&t), "%d/%b/%Y:%H:%M:%S %z");
            auto time_local_str  = ss.str();
            auto request         = std::format("{} {} {}", req->methodString(), req->path(), req->versionString());
            auto body_bytes_sent = res->getHeader("Content-Length");
            auto http_user_agent = req->getHeader("User-Agent");
            if (body_bytes_sent.empty()) body_bytes_sent = std::to_string(res->getBody().size());
            if (http_user_agent.empty()) http_user_agent = "unknown";

            // NOTE: From NGINX default access log format
            // log_format combined '$remote_addr - $remote_user [$time_local] '
            //                     '"$request" $status $body_bytes_sent '
            //                     '"$http_referer" "$http_user_agent"';

            constexpr auto log_html = R"({} - {} [{}] "{}" {} {} "{}" "{}")";
            // 127.0.0.1 - - [08/Apr/2025:12:07:01 +0000] "GET /livez HTTP/1.1" 200 3 "-" "curl/8.12.1"
            LOGGER_TRACE(std::format(log_html,
                req->peerAddr().toIp(), /*remote_user=*/"-", time_local_str,
                request, static_cast<int>(res->statusCode()),
                body_bytes_sent, /*http_referer=*/"-", http_user_agent));
        })
        // лимит размера тела запроса, защита от DDoS: 1 MB
        .setClientMaxBodySize(1 * 1024 * 1024)
        // максимальный размер сообщения от клиента WebSocket
        .setClientMaxWebSocketMessageSize(128 * 1024)
        // таймаут на поддерживание соединение без операций чтения/записи (секунд)
        // .setIdleConnectionTimeout(5)
        // Keep-Alive connection
        // .setKeepaliveRequestsNumber(2)
        // добавлять в каждый ответ заголовок "Data: Sat, 01 Jan 2005 11:00:00 GMT"
        .enableDateHeader(true)
        // добавлять в каждый ответ заголовок "Server: drogon/1.9.11"
        .enableServerHeader(true)
        // отключаем сжатие небинарного тела ответа (если его размер больше 1024 байт)
        .enableGzip(false)
        .enableBrotli(false)
        // настройки логирования (уровень Debug, с локальным временем, в stdout)
        .setLogLocalTime(true)
        .setLogLevel(trantor::Logger::LogLevel::kDebug)
        .setLogPath("")
        // количество потоков для IO event loops
        .setThreadNum(configuration.http_threads_count)
        // количество одновременно поддерживаемых подключений
        .setMaxConnectionNum(configuration.http_queue_capacity);

        // middleware
        http_server_->registerFilter(HttpMiddlewareAuth::create(service_auth_));

        // обработчики
        http_server_->registerHandler("/livez",
            [this](const drogon::HttpRequestPtr& /*req*/, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
                if (liveness_check_cb_
                &&  liveness_check_cb_()) {
                    auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k200OK, drogon::ContentType::CT_TEXT_PLAIN);
                    res->setBody("ok\n");
                    callback(res);
                } else {
                    auto res = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k500InternalServerError, drogon::ContentType::CT_TEXT_PLAIN);
                    res->setBody("fail\n");
                    callback(res);
                }
            },
            {drogon::HttpMethod::Get});

        http_server_->registerHandler("/readyz",
            [this](const drogon::HttpRequestPtr& /*req*/, std::function<void (const drogon::HttpResponsePtr&)>&& callback) {
                if (readiness_check_cb_
                &&  readiness_check_cb_()) {
                    auto res  = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k200OK, drogon::ContentType::CT_TEXT_PLAIN);
                    res->setBody("ok\n");
                    callback(res);
                } else {
                    auto res  = drogon::HttpResponse::newHttpResponse(drogon::HttpStatusCode::k500InternalServerError, drogon::ContentType::CT_TEXT_PLAIN);
                    res->setBody("fail\n");
                    callback(res);
                }
            },
            {drogon::HttpMethod::Get});

        NetHelpers::SocketAddress sock_addr(configuration.http_listening);
        http_server_->addListener(sock_addr.host().to_string(), sock_addr.port());

        std::string info;
        {
            std::map<std::string, std::string> enpdoints;
            auto handlers = http_server_->getHandlersInfo();
            for (const auto& handler : handlers) {
                std::string path_pattern;
                drogon::HttpMethod method;
                std::string description;
                std::tie(path_pattern, method, description) = handler;

                std::string enpdoint = std::format("{:8} {}", drogon::to_string(method), path_pattern);
                std::string key      = std::format("{}_{}", path_pattern, drogon::to_string(method));
                enpdoints[key] = enpdoint;
            }
            for (const auto& [_, enpdoint] : enpdoints) {
                info.append(std::format("\n  {}", enpdoint));
            }
        }
        LOGGER_INFOR(std::format("{} registered endpoints:{}", http_server_thread_name, info));

        http_server_thread_ = std::thread([this]()->void {
            // ThreadHelpers::block_signals();
            http_server_->run();
        });
        ThreadHelpers::set_name(http_server_thread_.native_handle(), http_server_thread_name);
    }
    catch (std::exception& ex) {
        LOGGER_ERROR(std::format("{} exception: {}", http_server_thread_name, ex.what()));
    }
}
