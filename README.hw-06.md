# Сервис социальной сети (курс Highload Architect)

## ДЗ 6: Онлайн обновление ленты новостей (очереди)

### Предварительная подготовка

Для проверки WebSocket соединения установить в систему утилиту **websocat**, для этого

```bash
curl -L https://github.com/vi/websocat/releases/download/v1.13.0/websocat.x86_64-unknown-linux-musl > websocat
chmod +x websocat
sudo mv websocat /usr/local/bin/
```

Далее можно устанавливать соединения следующей командой:

```bash
websocat -v ws://localhost:8080/post/feed/posted -H "Authorization: Bearer 7d7ea051-2a42-4aa6-bac6-c46143abb02e"
```

---

Для выполнения домашней работы в качестве брокера был выбран брокер сообщений Kafka. Для взаимодействия с ним из сервисов потребуется дополнительная библиотека **librdkafka** и для сборки и для выполнения. Поэтмоу потребуется пересобрать образ сборщика:

* перейти в каталог `cpp-builder/`
* выполнить скрипт `cpp-builder/prepare.sh`

По итогу соберется образ **alpine-cpp-builder:2**, со всеми библиотеками, необходимыми компиляторами, системами сборки и т.д.

---

Взаимодействовать через брокер будут два сервиса, монолитный **social_network** и новый микросервис **ws_service**:

* чтобы собрать сервис **social_network**, нужно выполнить скрипт `build.social_network.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **social_network:6**
* чтобы собрать сервис **ws_service**, нужно выполнить скрипт `build.ws_service.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **ws_service:1**

### Описание

По ДЗ нужно было разработать WebSocket сервер, при помощи которого подключенные клиенты будут сразу получать обновления постов своих друзей. Такой подход позволит сэкономить ресурсы и обновлять молниеносно ленты активных клиентов.

Контракт для асинхронного API находится в фале `misc/asyncapi.json`.

Был сформирован отдельный микроверсис **ws_service** (исходники в каталоге `service/ws_service`). Он представляет собой WebSocket-сервер с обработкой эндпойнта `/post/feed/posted` согласно контракту асинхронного API. Также он представляет собой KafkaConsumer, т.е. читающий из топика клиент.

Также был доработан монолитный сервис **social_network** (исходники в каталоге `service/social_network`). В него добавлен функционал KafkaProducer, т.е. пишущий в топик клиент.

Согласно ДЗ, при создании нового поста (через синхронный REST API эндпоинт `/post/create` сервиса **social_network**), новый пост должен быть как и ранее зарегистрирован в БД PostgreSQL, и отправлен в топик (очередь) брокера сообщений. Таким образом, материализация ленты постов происходит отложенно через очередь. Кроме того, обеспечивается отправка события нового поста только целевым пользователям (т.е. только друзьям), это происходит на этапе отправки сообщения в брокер, а не на этапе фильтрации на приемной стороне. А в свою очередь микросервис **ws_service** является потребителем из топика (очереди), когда через WebSocket к нему подключается клиент и аутентифицируется, то микросервис подписывается на топик с сообщениями для данного пользователя. И обновления ленты постов проходят в реальном времени (с минимальной задержкой и без необходимости выполнять синхронный REST запрос `/post/feed`).

### Подготовка

* развернуть систему

```bash
docker compose -f docker-compose.hw-06.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.hw-06.yml down --remove-orphans
```

В результате будет развернут ряд сервисов, среди них:

* `social_network:6`
* `ws_service:1`
* `kafka:3.9.0`
* `nginx:1.28.0-alpine`

Сервис **kafka** запущен без ZooKeeper (с использованием KRaft).

Микросервис **ws_service** запущен в трех экземплярах (**ws_srv1**, **ws_srv2**, **ws_srv3**), и доступ к ним проброшен через сервис **nginx** (простенький конфигурационный файл `ws_queue/nginx/nginx.conf`). Он выступает в качестве обратного прокси для трех backend WebSocket серверов с балансировкой нагрузки по методу наименьшего количества соединений (`least_conn`), а также выполняет терминирование протокола HTTP->WS.  
Таким образом клиенты всегда подключаются к единому адресу (**nginx**), а на самом деле происходит распределение нагрузки по конечным серверам. Клиенты не знают ничего о внутреннем устройстве системы.

В данной ДЗ Kafka запущен в виде одного инстанса. Это достаточно для проверки, но в идеале должен быть кластер, для масштабирования и отказоустойчивости.

Для этого можно воспользоваться механизмом KRaft, и при разворачивании нескольких инстансов, добавить в них настройки

* `- KAFKA_CFG_CONTROLLER_QUORUM_VOTERS=1@kafka1:9093,2@kafka2:9093,3@kafka3:9093`  
чтобы алгоритм понимал как получать кворум (обеспечение отказоустойчивости)
* `- KAFKA_CFG_DEFAULT_REPLICATION_FACTOR=3`  
чтобы данные реплицировались по экземплярам и не терялись при выходе из строя одного из них (защита от потери данных)
* `- KAFKA_CFG_MIN_INSYNC_REPLICAS=2`  
чтобы быть уверенными, что данные успешно перенесены хотя бы на несколько реплик (гарантии записи)
* `- KAFKA_CFG_NUM_PARTITIONS=100`  
больше партиций для лучшего распределения данных (лучше для высоких нагрузок)

Также в микросервисе надо прописать URL не одного брокера, а нескольких, чтобы KafkaConsumer мог подключаться к другому инстансу, если один из них не доступен.

Также наверное будет полезно затюнить настройки консьюмера, но в текущий момент я очень мало знаю про Kafka. Настроек там очень много, надо с ним разобраться.

### Проверка

* развернем систему
* сразу можно посмотреть на вывод логов одного из сервисов вебсокетов, там видно какие настройки у KafkaConsumer

```text
2025-08-21 11:41:59.734533 [INFOR] (app.cpp:297) :: HttpSrv registered endpoints:
  GET      /livez
  GET      /post/feed/posted
  GET      /readyz
### Global config
builtin.features = gzip,snappy,ssl,sasl,regex,lz4,sasl_gssapi,sasl_plain,sasl_scram,plugins,zstd,sasl_oauthbearer
client.id = ws_srv1
client.software.name = librdkafka
metadata.broker.list = kafka:9092
message.max.bytes = 1000000
message.copy.max.bytes = 65535
receive.message.max.bytes = 100000000
max.in.flight.requests.per.connection = 1000000
metadata.request.timeout.ms = 10
topic.metadata.refresh.interval.ms = 300000
metadata.max.age.ms = 900000
topic.metadata.refresh.fast.interval.ms = 100
topic.metadata.refresh.fast.cnt = 10
topic.metadata.refresh.sparse = true
topic.metadata.propagation.max.ms = 30000
debug = 
socket.timeout.ms = 60000
socket.blocking.max.ms = 1000
socket.send.buffer.bytes = 0
socket.receive.buffer.bytes = 0
socket.keepalive.enable = false
socket.nagle.disable = false
socket.max.fails = 1
broker.address.ttl = 1000
broker.address.family = any
socket.connection.setup.timeout.ms = 30000
connections.max.idle.ms = 0
enable.sparse.connections = true
reconnect.backoff.jitter.ms = 0
reconnect.backoff.ms = 100
reconnect.backoff.max.ms = 10000
statistics.interval.ms = 0
enabled_events = 0
log_cb = 0x7f091a88cba0
log_level = 6
log.queue = false
log.thread.name = true
enable.random.seed = true
log.connection.close = true
socket_cb = 0x7f091a8af9b0
open_cb = 0x7f091a8fd610
internal.termination.signal = 0
api.version.request = true
api.version.request.timeout.ms = 10000
api.version.fallback.ms = 0
broker.version.fallback = 0.10.0
allow.auto.create.topics = false
security.protocol = plaintext
ssl.ca.certificate.stores = Root
ssl.engine.id = dynamic
enable.ssl.certificate.verification = true
ssl.endpoint.identification.algorithm = https
sasl.mechanisms = GSSAPI
sasl.kerberos.service.name = kafka
sasl.kerberos.principal = kafkaclient
sasl.kerberos.kinit.cmd = kinit -R -t "%{sasl.kerberos.keytab}" -k %{sasl.kerberos.principal} || kinit -t "%{sasl.kerberos.keytab}" -k %{sasl.kerberos.principal}
sasl.kerberos.min.time.before.relogin = 60000
enable.sasl.oauthbearer.unsecure.jwt = false
enable_sasl_queue = false
sasl.oauthbearer.method = default
test.mock.num.brokers = 0
test.mock.broker.rtt = 0
group.id = ws_service_group
partition.assignment.strategy = roundrobin
session.timeout.ms = 45000
heartbeat.interval.ms = 3000
group.protocol.type = consumer
coordinator.query.interval.ms = 600000
max.poll.interval.ms = 300000
enable.auto.commit = true
auto.commit.interval.ms = 1000
enable.auto.offset.store = true
queued.min.messages = 100000
queued.max.messages.kbytes = 65536
fetch.wait.max.ms = 500
fetch.queue.backoff.ms = 1000
fetch.message.max.bytes = 1048576
fetch.max.bytes = 52428800
fetch.min.bytes = 1
fetch.error.backoff.ms = 500
offset.store.method = broker
isolation.level = read_committed
enable.partition.eof = false
check.crcs = false
client.rack = 
transaction.timeout.ms = 60000
enable.idempotence = false
enable.gapless.guarantee = false
queue.buffering.max.messages = 100000
queue.buffering.max.kbytes = 1048576
queue.buffering.max.ms = 5
message.send.max.retries = 2147483647
retry.backoff.ms = 100
retry.backoff.max.ms = 1000
queue.buffering.backpressure.threshold = 1
compression.codec = none
batch.num.messages = 10000
batch.size = 1000000
delivery.report.only.error = false
sticky.partitioning.linger.ms = 10
client.dns.lookup = use_all_dns_ips

2025-08-21 11:41:59.926130 [INFOR] (kafka_client_consumer.cpp:72) :: Created Kafka consumer 'ws_srv1#consumer-1'
```

* также посмотрим на вывод логов основного сервиса, с настройками KafkaProducer

```text
2025-08-21 11:41:59.964377 [DEBUG] (configuration.cpp:474) :: configuration:
  pgsql_master.url="postgresql://postgres_db:5432/postgres"
  pgsql_master.login=postgres
  pgsql_master.password=pgpass
  grpc.url="grpc://chat_srv:50051"
  kafka.url="tcp://kafka:9092"
  http.listening="0.0.0.0:6000"
  http.threads_count=1
  http.queue_capacity=4096
  prometheus.listening=0.0.0.0:6001
### Global config
builtin.features = gzip,snappy,ssl,sasl,regex,lz4,sasl_gssapi,sasl_plain,sasl_scram,plugins,zstd,sasl_oauthbearer
client.id = social_srv
client.software.name = librdkafka
metadata.broker.list = kafka:9092
message.max.bytes = 1000000
message.copy.max.bytes = 65535
receive.message.max.bytes = 100000000
max.in.flight.requests.per.connection = 1000000
metadata.request.timeout.ms = 10
topic.metadata.refresh.interval.ms = 300000
metadata.max.age.ms = 900000
topic.metadata.refresh.fast.interval.ms = 100
topic.metadata.refresh.fast.cnt = 10
topic.metadata.refresh.sparse = true
topic.metadata.propagation.max.ms = 30000
debug = 
socket.timeout.ms = 60000
socket.blocking.max.ms = 1000
socket.send.buffer.bytes = 0
socket.receive.buffer.bytes = 0
socket.keepalive.enable = false
socket.nagle.disable = false
socket.max.fails = 1
broker.address.ttl = 1000
broker.address.family = any
socket.connection.setup.timeout.ms = 30000
connections.max.idle.ms = 0
enable.sparse.connections = true
reconnect.backoff.jitter.ms = 0
reconnect.backoff.ms = 100
reconnect.backoff.max.ms = 10000
statistics.interval.ms = 0
enabled_events = 0
log_cb = 0x7fd50c354ba0
log_level = 6
log.queue = false
log.thread.name = true
enable.random.seed = true
log.connection.close = true
socket_cb = 0x7fd50c3779b0
open_cb = 0x7fd50c3c5610
default_topic_conf = 0x7fd50c0827f0
internal.termination.signal = 0
api.version.request = true
api.version.request.timeout.ms = 10000
api.version.fallback.ms = 0
broker.version.fallback = 0.10.0
allow.auto.create.topics = false
security.protocol = plaintext
ssl.ca.certificate.stores = Root
ssl.engine.id = dynamic
enable.ssl.certificate.verification = true
ssl.endpoint.identification.algorithm = https
sasl.mechanisms = GSSAPI
sasl.kerberos.service.name = kafka
sasl.kerberos.principal = kafkaclient
sasl.kerberos.kinit.cmd = kinit -R -t "%{sasl.kerberos.keytab}" -k %{sasl.kerberos.principal} || kinit -t "%{sasl.kerberos.keytab}" -k %{sasl.kerberos.principal}
sasl.kerberos.min.time.before.relogin = 60000
enable.sasl.oauthbearer.unsecure.jwt = false
enable_sasl_queue = false
sasl.oauthbearer.method = default
test.mock.num.brokers = 0
test.mock.broker.rtt = 0
partition.assignment.strategy = range,roundrobin
session.timeout.ms = 45000
heartbeat.interval.ms = 3000
group.protocol.type = consumer
coordinator.query.interval.ms = 600000
max.poll.interval.ms = 300000
enable.auto.commit = true
auto.commit.interval.ms = 5000
enable.auto.offset.store = true
queued.min.messages = 100000
queued.max.messages.kbytes = 65536
fetch.wait.max.ms = 500
fetch.queue.backoff.ms = 1000
fetch.message.max.bytes = 1048576
fetch.max.bytes = 52428800
fetch.min.bytes = 1
fetch.error.backoff.ms = 500
offset.store.method = broker
isolation.level = read_committed
enable.partition.eof = false
check.crcs = false
client.rack = 
transaction.timeout.ms = 60000
enable.idempotence = false
enable.gapless.guarantee = false
queue.buffering.max.messages = 100000
queue.buffering.max.kbytes = 1048576
queue.buffering.max.ms = 5
message.send.max.retries = 5
retry.backoff.ms = 100
retry.backoff.max.ms = 1000
queue.buffering.backpressure.threshold = 1
compression.codec = snappy
batch.num.messages = 10000
batch.size = 1000000
delivery.report.only.error = false
sticky.partitioning.linger.ms = 10
client.dns.lookup = use_all_dns_ips

2025-08-21 11:42:00.053690 [INFOR] (kafka_client_producer.cpp:67) :: Created Kafka producer social_srv#producer-1
```

* используем несколько UUID для теста

```bash
docker exec -it postgres_db psql -U postgres -c "SELECT id FROM users LIMIT 6;"
                  id                  
--------------------------------------
 f776b179-828d-47d9-937d-66ab8a9de5b2
 d6f778e1-c685-4393-8a7b-abef8596e3e5
 2f8d9273-3fb3-48ef-b98b-310dacf79316
 dc3e666d-97cd-46cf-b151-d942022f435e
 e5aedd63-6105-4174-a75c-30fdd511fe15
 cf093a1f-2e19-41b6-bcb0-db7d4db793ba
(6 rows)
```

* создадим отношения дружбы между **f776b179-828d-47d9-937d-66ab8a9de5b2** и **cf093a1f-2e19-41b6-bcb0-db7d4db793ba**

```bash
curl -X PUT http://localhost:6000/friend/set/cf093a1f-2e19-41b6-bcb0-db7d4db793ba \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d ''
```

* для удобства запустим просмотр логов из всех экземпляров сервиса вебсокетов

```bash
docker compose -f docker-compose.hw-06.yml logs -f ws_srv1 ws_srv2 ws_srv3
```

* теперь создадим пост. отправим новость от **f776b179-828d-47d9-937d-66ab8a9de5b2**

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d '{"text": "новость 1"}'
```

* проверим логи сервиса **social_network**.  
убедимся, что прошла аутентификация пользователя, был зарегистрирован новый пост в PostgreSQL, после успешного завершения, мы запросили из БД список друзей пользователя (в данном случае у нас только один друг), а затем отправили сообщение в топик для соответствующего друга (формат названия топика `user_<uuid>_posts`)

```text
2025-08-22 07:54:09.420005 [TRACE] (app_database_service.cpp:17) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 07:54:09.421199 [DEBUG] (http_post_service.cpp:62) :: handler: POST /post/create
2025-08-22 07:54:09.421267 [TRACE] (app_database_service.cpp:316) :: create_post: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 07:54:09.440660 [TRACE] (app_database_service.cpp:279) :: get_friends: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 07:54:09.441638 [DEBUG] (kafka_client_producer.cpp:83) :: KafkaProducer produce: enqueued message (157 byte(s)) for topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts'
2025-08-22 07:54:09.441730 [TRACE] (app.cpp:289) :: 172.21.0.1 - - [22/Aug/2025:07:54:09 +0000] "POST /post/create HTTP/1.1" 200 50 "-" "curl/7.68.0"
```

* убедимся, что в ленте друга **cf093a1f-2e19-41b6-bcb0-db7d4db793ba** действительно есть посты через REST запрос

```bash
curl -v -X GET 'http://localhost:6000/post/feed?offset=0&limit=2' \
  -H "Authorization: Bearer cf093a1f-2e19-41b6-bcb0-db7d4db793ba" | jq .

[
  {
    "author_user_id": "f776b179-828d-47d9-937d-66ab8a9de5b2",
    "id": "faa3d200-a8b7-410f-8bfb-958011c05c01",
    "text": "новость 1"
  }
]
```

* теперь подключаемся через WebSocket к сервису **ws_service** на эндпоинт `/post/feed/posted` и порт **nginx**. авторизуемся от имени друга **cf093a1f-2e19-41b6-bcb0-db7d4db793ba** и будем следить за постами в реальном времени.  
будет висеть соединение в терминале и ожидать входящих данных (периодически отсылаются ping/pong для подержания соединения)

```bash
websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer cf093a1f-2e19-41b6-bcb0-db7d4db793ba"
```

* в логах сервисов вебсокетов видим, что **nginx** направил соедиение на экземпляр **ws_srv1**.  
убеждаемся, что соедиение установлено, аутентификация прошла, и KafkaConsumer подписался на топик **user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts**

```text
ws_srv1  | 2025-08-22 08:03:50.438988 [TRACE] (app.cpp:211) :: 172.21.0.11 - - [22/Aug/2025:08:03:50 +0000] "GET /post/feed/posted HTTP/1.1" 101 0 "-" "unknown"
ws_srv1  | 2025-08-22 08:03:50.439342 [INFOR] (ws_controller_post.cpp:80) :: <instance_addr=172.21.0.8:6000> websocket: user_id=cf093a1f-2e19-41b6-bcb0-db7d4db793ba, start processing
ws_srv1  | 2025-08-22 08:03:50.439435 [DEBUG] (kafka_client_consumer.cpp:89) :: KafkaConsumer: subscribe to topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts'
```

* создадим еще одну новость от пользователя **f776b179-828d-47d9-937d-66ab8a9de5b2**

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d '{"text": "новость 2"}'
```

* в логах сервиса **social_network** видим, что пост создан и отправлен в топик друга `user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts`

```text
2025-08-22 08:06:46.447071 [TRACE] (app_database_service.cpp:17) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 08:06:46.448161 [DEBUG] (http_post_service.cpp:62) :: handler: POST /post/create
2025-08-22 08:06:46.448221 [TRACE] (app_database_service.cpp:316) :: create_post: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 08:06:46.452058 [TRACE] (app_database_service.cpp:279) :: get_friends: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 08:06:46.453121 [DEBUG] (kafka_client_producer.cpp:83) :: KafkaProducer produce: enqueued message (157 byte(s)) for topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts'
2025-08-22 08:06:46.453161 [DEBUG] (kafka_client_producer.cpp:16) :: KafkaDeliveryReporter: message delivered to topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts' [1] at offset 0
2025-08-22 08:06:46.453245 [TRACE] (app.cpp:289) :: 172.21.0.1 - - [22/Aug/2025:08:06:46 +0000] "POST /post/create HTTP/1.1" 200 50 "-" "curl/7.68.0"
```

* в логах **ws_srv1** видим, что сообщение из топика `user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts` получено и обработано

```text
ws_srv1  | 2025-08-22 08:06:46.476700 [DEBUG] (kafka_client_consumer.cpp:143) :: KafkaConsumer thread: read message at offset 0, key: ??, timestamp: create time 1755850006453
```

* а в консоли клиента вебсокетов видим прилетевшие данные.  
убеждаемся, что WebSocket работает корректно

```text
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 2","created_ms":1755850006448,"post_id":"dfb6bbd6-8274-40ec-8a49-66777a3a79dd"}
```

* добавим для пользователя **f776b179-828d-47d9-937d-66ab8a9de5b2** нового друга **e5aedd63-6105-4174-a75c-30fdd511fe15**

```bash
curl -X PUT http://localhost:6000/friend/set/e5aedd63-6105-4174-a75c-30fdd511fe15 \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d ''
```

* проверим ленту новостей нового друга **e5aedd63-6105-4174-a75c-30fdd511fe15**, ожидаемо там есть несколько сообщений

```bash
curl -v -X GET 'http://localhost:6000/post/feed?offset=0&limit=2' \
  -H "Authorization: Bearer e5aedd63-6105-4174-a75c-30fdd511fe15" | jq .

[
  {
    "author_user_id": "f776b179-828d-47d9-937d-66ab8a9de5b2",
    "id": "dfb6bbd6-8274-40ec-8a49-66777a3a79dd",
    "text": "новость 2"
  },
  {
    "author_user_id": "f776b179-828d-47d9-937d-66ab8a9de5b2",
    "id": "faa3d200-a8b7-410f-8bfb-958011c05c01",
    "text": "новость 1"
  }
]
```

* подключаемся новым другом к WebSocket каналу `/post/feed/posted`.  
ожидаемо, в терминале не видим никаких сообщений, т.к. будут прилетать только одновления

```bash
websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer e5aedd63-6105-4174-a75c-30fdd511fe15"
```

* и еще одним пользователем **dc3e666d-97cd-46cf-b151-d942022f435e** подключимся к WebSocket каналу `/post/feed/posted`. это не друг, просто пользователь

```bash
websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer dc3e666d-97cd-46cf-b151-d942022f435e"
```

* по логам наблюдаем, что **nginx** направил первое новое соединение в инстанс **ws_srv2**, а второе новое соединение в инстанс **ws_srv3**

```text
ws_srv2  | 2025-08-22 08:17:09.520873 [TRACE] (app.cpp:211) :: 172.21.0.11 - - [22/Aug/2025:08:17:09 +0000] "GET /post/feed/posted HTTP/1.1" 101 0 "-" "unknown"
ws_srv2  | 2025-08-22 08:17:09.521219 [INFOR] (ws_controller_post.cpp:80) :: <instance_addr=172.21.0.10:6000> websocket: user_id=e5aedd63-6105-4174-a75c-30fdd511fe15, start processing
ws_srv2  | 2025-08-22 08:17:09.521334 [DEBUG] (kafka_client_consumer.cpp:89) :: KafkaConsumer: subscribe to topic 'user_e5aedd63-6105-4174-a75c-30fdd511fe15_posts'
ws_srv3  | 2025-08-22 09:11:28.639941 [TRACE] (app.cpp:211) :: 172.21.0.11 - - [22/Aug/2025:09:11:28 +0000] "GET /post/feed/posted HTTP/1.1" 101 0 "-" "unknown"
ws_srv3  | 2025-08-22 09:11:28.640325 [INFOR] (ws_controller_post.cpp:80) :: <instance_addr=172.21.0.9:6000> websocket: user_id=dc3e666d-97cd-46cf-b151-d942022f435e, start processing
ws_srv3  | 2025-08-22 09:11:28.640435 [DEBUG] (kafka_client_consumer.cpp:89) :: KafkaConsumer: subscribe to topic 'user_dc3e666d-97cd-46cf-b151-d942022f435e_posts'
```

* отправим новый пост с новостью от пользователя **f776b179-828d-47d9-937d-66ab8a9de5b2**

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d '{"text": "новость 3"}'
```

* наблюдаем в логах сервиса **social_network**, что пост направлен в соответсвующие топики друзей

```text
2025-08-22 09:13:45.885595 [DEBUG] (http_post_service.cpp:62) :: handler: POST /post/create
2025-08-22 09:13:45.885676 [TRACE] (app_database_service.cpp:316) :: create_post: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 09:13:45.889399 [TRACE] (app_database_service.cpp:279) :: get_friends: query to MASTER #0 tag='postgres_db:5432'
2025-08-22 09:13:45.890341 [DEBUG] (kafka_client_producer.cpp:83) :: KafkaProducer produce: enqueued message (157 byte(s)) for topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts'
2025-08-22 09:13:45.890374 [DEBUG] (kafka_client_producer.cpp:83) :: KafkaProducer produce: enqueued message (157 byte(s)) for topic 'user_e5aedd63-6105-4174-a75c-30fdd511fe15_posts'
2025-08-22 09:13:45.890415 [DEBUG] (kafka_client_producer.cpp:16) :: KafkaDeliveryReporter: message delivered to topic 'user_cf093a1f-2e19-41b6-bcb0-db7d4db793ba_posts' [6] at offset 0
```

* наблюдаем в логах микросервисов вебсокетов, что два экземпляра сервиса **ws_srv1** и **ws_srv2** получили и обработали сообщения, а третий **ws_srv3** ничего не получал, потому что в его топик ничего не было отправлено, потмоу что его клиент - не является другом для пользователя **f776b179-828d-47d9-937d-66ab8a9de5b2**

```text
ws_srv1  | 2025-08-22 09:13:45.897412 [DEBUG] (kafka_client_consumer.cpp:143) :: KafkaConsumer thread: read message at offset 1, key: ??, timestamp: create time 1755854025890
ws_srv2  | 2025-08-22 09:13:48.363416 [DEBUG] (kafka_client_consumer.cpp:143) :: KafkaConsumer thread: read message at offset 0, key: ??, timestamp: create time 1755854025890
```

* ожидаемо, наблюдаем как выглядят три терминала с клиентами вебсокетов.  
в каналы прилетают только обновения новостей в реальном времени (с очень малой задержкой времени)

```bash
# первый клиент
$ websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer cf093a1f-2e19-41b6-bcb0-db7d4db793ba"
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 2","created_ms":1755850006448,"post_id":"dfb6bbd6-8274-40ec-8a49-66777a3a79dd"}
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 3","created_ms":1755854025886,"post_id":"22845f59-63f1-4227-ac7c-6539bccd9846"}


# второй клиент
$ websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer e5aedd63-6105-4174-a75c-30fdd511fe15"
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 3","created_ms":1755854025886,"post_id":"22845f59-63f1-4227-ac7c-6539bccd9846"}


# третий клиент
$ websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer dc3e666d-97cd-46cf-b151-d942022f435e"

```

* отключим второй и третий WS-клиент

```text
ws_srv2  | 2025-08-22 09:22:44.219684 [INFOR] (ws_controller_post.cpp:99) :: <instance_addr=172.21.0.10:6000> websocket: user_id=e5aedd63-6105-4174-a75c-30fdd511fe15, stop processing
ws_srv2  | 2025-08-22 09:22:44.219755 [DEBUG] (kafka_client_consumer.cpp:106) :: KafkaConsumer: unsubscribe from topic 'user_e5aedd63-6105-4174-a75c-30fdd511fe15_posts'
ws_srv3  | 2025-08-22 09:22:45.962581 [INFOR] (ws_controller_post.cpp:99) :: <instance_addr=172.21.0.9:6000> websocket: user_id=dc3e666d-97cd-46cf-b151-d942022f435e, stop processing
ws_srv3  | 2025-08-22 09:22:45.962617 [DEBUG] (kafka_client_consumer.cpp:106) :: KafkaConsumer: unsubscribe from topic 'user_dc3e666d-97cd-46cf-b151-d942022f435e_posts'
```

* остановим экземпляры **ws_srv2** и **ws_srv3**, оставим только первый экземпляр

```bash
docker compose -f docker-compose.hw-06.yml stop ws_srv2 ws_srv3
[+] Stopping 2/2
 ✔ Container ws_srv3  Stopped              10.9s 
 ✔ Container ws_srv2  Stopped              10.9s 
```

* подключимся вторым клиентом снова.  
по логам сервисов наблюдаем, что с большой задержкой, но **nginx** всё таки раздуплился и подключил новое соединение на **ws_srv1** (ему потребовалось порядка 5 секунд - чтобы понять, что у него нет доступа к **ws_srv2**, затем порядка 5 секунд - чтобы понять, что у него больше нет доступа к **ws_srv3**).

```text
ws_srv2 exited with code 137
ws_srv3 exited with code 137
ws_srv1  | 2025-08-22 09:26:05.519001 [TRACE] (app.cpp:211) :: 172.21.0.11 - - [22/Aug/2025:09:26:05 +0000] "GET /post/feed/posted HTTP/1.1" 101 0 "-" "unknown"
ws_srv1  | 2025-08-22 09:26:05.519058 [INFOR] (ws_controller_post.cpp:80) :: <instance_addr=172.21.0.8:6000> websocket: user_id=e5aedd63-6105-4174-a75c-30fdd511fe15, start processing
ws_srv1  | 2025-08-22 09:26:05.519179 [DEBUG] (kafka_client_consumer.cpp:89) :: KafkaConsumer: subscribe to topic 'user_e5aedd63-6105-4174-a75c-30fdd511fe15_posts'
```

* отправим пользователем **f776b179-828d-47d9-937d-66ab8a9de5b2** еще одну новость

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer f776b179-828d-47d9-937d-66ab8a9de5b2" \
  -d '{"text": "новость 4"}'
```

* в логах наблюдаем, что экземпляр **ws_srv1**, к которому теперь подсоединены оба клиента мгновенно получил сообщения из двух топиков двух клиентов, и обработал их

```text
ws_srv1  | 2025-08-22 09:30:21.996467 [DEBUG] (kafka_client_consumer.cpp:143) :: KafkaConsumer thread: read message at offset 0, key: ??, timestamp: create time 1755855021989
ws_srv1  | 2025-08-22 09:30:21.997281 [DEBUG] (kafka_client_consumer.cpp:143) :: KafkaConsumer thread: read message at offset 1, key: ??, timestamp: create time 1755855021989
```

* терминалы WS-клиентов выглядят так

```bash
# первый клиент
$ websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer cf093a1f-2e19-41b6-bcb0-db7d4db793ba"
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 2","created_ms":1755850006448,"post_id":"dfb6bbd6-8274-40ec-8a49-66777a3a79dd"}
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 3","created_ms":1755854025886,"post_id":"22845f59-63f1-4227-ac7c-6539bccd9846"}
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 4","created_ms":1755855021985,"post_id":"1ccf92c2-6e70-40ad-8d90-bcda8f044fef"}

# второй клиент
$ websocat ws://localhost:8080/post/feed/posted -H "Authorization: Bearer e5aedd63-6105-4174-a75c-30fdd511fe15"
{"author_id":"f776b179-828d-47d9-937d-66ab8a9de5b2","content":"новость 4","created_ms":1755855021985,"post_id":"1ccf92c2-6e70-40ad-8d90-bcda8f044fef"}

```

* отметим, что корректно работают WebSocket сервера, работа обновления в реальном времени лент новостей через топики Kafka также работает корректно, и балансировка нагрузки на несколько экземпляров серверов через Nginx также выполняется корректно

### Выводы

* Новый сервис вебсокетов работает корректно.
* При добавлении поста у друга, лента обновляется автоматически (с небольшой задержкой).
* Архитектура микросервиса обеспечивает линейное масштабирование.  
Это достигается тем, что сервис **ws_service** является stateless, т.е. у него нет явного состояния между инстансами. Клиент подключается к сервису, в этот момент происходит подписка на конкретный топик в брокере.
* В отчете описал и текущую архитектуру, и возможный процесс масштабирования Kafka.
