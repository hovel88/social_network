# Сервис социальной сети (курс Highload Architect)

## ДЗ 10: SAGA

### Описание

По постановке ДЗ, требуется:

* разработать сервис счетчиков (лайки на посты)
* использование паттерна SAGA для обеспечения консистентности между несколькими сервисами
* внедрить отображение счетчиков

---

Взаимодействовать будут сервисы: монолитный **social_network**, микросервис реалтайм отображения данных через WebSocket **ws_service**, а также новый микросервис для счетчика лайков **likes_service**.

* чтобы собрать сервис **social_network**, нужно выполнить скрипт `build.social_network.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **social_network:10**
* чтобы собрать сервис **ws_service**, нужно выполнить скрипт `build.ws_service.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **ws_service:2**
* чтобы собрать сервис **likes_service**, нужно выполнить скрипт `build.likes_service.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **likes_service:1**

---

Команда для разворачивания системы

```bash
docker compose -f docker-compose.hw-10.yml up -d
```

Команда для остановки системы

```bash
docker compose -f docker-compose.hw-10.yml down --remove-orphans
```

---

Для начала необходимо развернуть систему, зайти в БД (не из хостовой системы, потому что на хосте версия 12, а у нас 16, с этим есть некие проблемы!). 

```bash
docker run -it --rm --network social_network_net --name pg postgres:16-alpine /bin/sh
psql --host haproxy_lb --port 5000 -U postgres -d postgres
```

Выполнить обновление схемы таблицы **posts** (итоговую схему можно посмотреть в `misc/db_posts.sql`)

```sql
--
-- добавляем новое поле
-- и обновляем все имеющиеся записи
--
ALTER TABLE IF EXISTS posts 
ADD COLUMN IF NOT EXISTS 
    likes_count INTEGER     DEFAULT 0;

UPDATE posts SET likes_count = 0 WHERE likes_count IS NULL;
```

Далее, добавить новую таблицу **post_likes** (см в файле `misc/db_likes.sql`)

```sql
--
-- таблица сведений о лайках (кто поставил, на какой пост, когда, в рамках какой SAGA)
--
CREATE TABLE IF NOT EXISTS post_likes (
    id          BIGSERIAL    PRIMARY KEY,
    user_id     UUID         NOT NULL,
    post_id     UUID         NOT NULL,
    saga_id     VARCHAR(100) NOT NULL,
    op_type     VARCHAR(20)  NOT NULL,
    created_at  TIMESTAMP    DEFAULT NOW()
);

CREATE INDEX idx_post_likes_post_id ON post_likes(post_id);
CREATE INDEX idx_post_likes_user_id ON post_likes(user_id);
CREATE INDEX idx_post_likes_saga_id ON post_likes(saga_id);
```

На этом подготовка БД завершена.

---

Сформулируем распределенную SAGA.

SAGA будет объединять микросервис **likes_service** (он же оркестратор) с **social_network** по протоколу gRPC. Внутри **social_network** доступ к таблице постов, а также таблицам пользователей и друзей. В таблице постов для каждого поста имеется счетчик лайков. Протокол gRPC можно посмотреть в файле `service/pb/likes_service.proto`. Соответствующий gRPC-сервер реализован в **social_network**.

Сам **likes_service** реализует набор REST endpoint'ов для взаимодействия с сервисом лайков:

```bash
likes_srv   | 2025-11-14 11:14:54.945809 [INFOR] (app.cpp:302) :: HttpSrv registered endpoints:
likes_srv   |   DELETE   /likes/([0-9a-fA-F-]{36})
likes_srv   |   GET      /likes/([0-9a-fA-F-]{36})
likes_srv   |   POST     /likes/([0-9a-fA-F-]{36})
likes_srv   |   GET      /likes/details/([0-9a-fA-F-]{36})
```

Кроме оркестрации и управления SAGA у **likes_service** есть доступ к БД в таблицу лайков для постов (**post_likes**), в этой таблице хранится вся история о том, кто когда, на какой пост поставил лайк или удалил лайк. Выглядит это так:

```bash
postgres=# select * from post_likes;
 id |               user_id                |               post_id                |                    saga_id                     |  op_type  |         created_at         
----+--------------------------------------+--------------------------------------+------------------------------------------------+-----------+----------------------------
  1 | b1fdfc3f-b531-40a9-8168-f65e94cec30c | 4e104092-5fc5-4f29-a763-ce43300211ab | like_saga_f7c9bfa3-bad7-423e-b701-a8fb6560bbd7 | DECREMENT | 2025-11-14 11:15:54.964056
  2 | b1fdfc3f-b531-40a9-8168-f65e94cec30c | 4e104092-5fc5-4f29-a763-ce43300211ab | like_saga_02129766-4dbf-418d-9c9a-a6176193114b | INCREMENT | 2025-11-14 11:16:33.188952
  3 | b1fdfc3f-b531-40a9-8168-f65e94cec30c | 4e104092-5fc5-4f29-a763-ce43300211ab | like_saga_4e5190c9-ad42-4494-856a-210f630032cd | DECREMENT | 2025-11-14 11:17:43.091424
  4 | b1fdfc3f-b531-40a9-8168-f65e94cec30c | 4e104092-5fc5-4f29-a763-ce43300211ab | like_saga_c10a6358-6f2c-4125-9052-b525482c801d | INCREMENT | 2025-11-14 11:17:58.886528
  5 | b1fdfc3f-b531-40a9-8168-f65e94cec30c | 4e104092-5fc5-4f29-a763-ce43300211ab | like_saga_e79686e7-4844-4349-94ef-74ec36a7befb | INCREMENT | 2025-11-14 11:18:03.105712
(5 rows)
```

SAGA также объединяет микросервис **likes_service** с **ws_service** через топики Kafka. Уже исползовался топик формата `user_3b287365-ac70-47e1-b9ba-40f56dc3dc7e_posts`, для отображения постов от друзей. Теперь используется новый топик формата `user_3b287365-ac70-47e1-b9ba-40f56dc3dc7e_post_likes`, в который будут отсылаться все обновления об изменении счетчиков для постов друзей. Т.е. за отображение информации о счетчиках лайков на пост, был выбран ранее уже использовавшийся **ws_service**, в него по Kafka будут передаваться все изменения счетчиков, а он будет отображать их в режиме реалтайм по WebSocket.

Таким образом, SAGA у нас увязывает 3 сервиса и ряд разных технологий для обеспечения консистентности распределенной транзакции.

Реализация оркестрации SAGA описано в файле `service/likes_service/sources/likes_saga.cpp`. Реализовано два сценария SAGA: добавление лайка пользователем на пост, и удаление пользователем лайка с поста.

SAGA состоит их четырех шагов:

1. **likes_service** формирует в локальной БД в таблице **post_likes** запись о действии INCREMENT (или DECREMENT)
2. Выполняется обращение по gRPC к сервису **social_network**, чтобы в таблице **posts** для поста выполнить увеличение (или уменьшение) счетчика и получить обновленное значение
3. Выполняется запрос по gRPC к сервису **social_network**, чтобы выяснить кто является автором поста и затем из таблицы **friends** извлечь список ID друзей автора. Этот список возвращается в запросе.
4. Формируется список Kafka топиков на основе массива ID друзей автора поста. Во все топики друзей отсылается JSON сообщение о изменении состояния счетчиков (затем через Kafka данные будут получены в **ws_service** и отобразятся клиентам по WebSocket).

SAGA подразумевает компенсационные шаги, но не для всех действий:

* шаг 4 **не нуждается в компенсации**, он идемпотентен, послали в топик и забыли
* шаг 3 **не нуждается в компенсации**, это Read-only операция
* шаг 2 **имеет компенсацию** в виде противоположного действия: для запроса увеличения счетчика - выполняется компенсирующий запрос на уменьшение счетчика и наоборот
* шаг 1 **имеет компенсацию** в виде удаления из таблицы **post_likes** всех записей для определенной SAGA ID, которая сфейлилась.

Ниже для наглядности приведена потоковая диаграмма взаимодействия сервисов при необходимости компенсационных действий:
<br>![SAGA с компенсацией](./misc/saga_compensation.svg)<br>

Ниже для наглядности приведена потоковая диаграмма взаимодействия сервисов при успешном пути:
<br>![Успешная SAGA](./misc/saga_ok.svg)<br>

### Проверка

* в БД посмотреть список ID пользователей

```bash
postgres=# SELECT id FROM users LIMIT 6;
                  id                  
--------------------------------------
 5d585f81-37c5-4afa-9cb1-8da736a8a8eb
 ff2eccca-58c3-434c-8081-5e87a2504be9
 8e5f9c9b-843a-4440-82e5-a669a2c09fa1
 9a6e5dad-2170-4575-9b1f-41e2e14ed8cd
 3b287365-ac70-47e1-b9ba-40f56dc3dc7e
 b1fdfc3f-b531-40a9-8168-f65e94cec30c
(6 rows)
```

* создать дружбу между **5d585f81-37c5-4afa-9cb1-8da736a8a8eb**, **3b287365-ac70-47e1-b9ba-40f56dc3dc7e**, **b1fdfc3f-b531-40a9-8168-f65e94cec30c**

```bash
curl -X PUT http://localhost:6000/friend/set/b1fdfc3f-b531-40a9-8168-f65e94cec30c \
  -H "Authorization: Bearer 5d585f81-37c5-4afa-9cb1-8da736a8a8eb" \
  -d ''

curl -X PUT http://localhost:6000/friend/set/3b287365-ac70-47e1-b9ba-40f56dc3dc7e \
  -H "Authorization: Bearer 5d585f81-37c5-4afa-9cb1-8da736a8a8eb" \
  -d ''
```

* включаем наблюдение логов

```bash
docker compose -f docker-compose.hw-10.yml logs -f ws_srv social_srv likes_srv
```

* публикуем новость

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer 5d585f81-37c5-4afa-9cb1-8da736a8a8eb" \
  -d '{"text": "новость 1"}'
{"post_id":"4e104092-5fc5-4f29-a763-ce43300211ab"}
```

* убираем лайк (убеждаемся, что не станет меньше 0)

```bash
curl -X DELETE http://localhost:9000/likes/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
{"message":"All SAGA steps completed successfully","saga_id":"like_saga_f7c9bfa3-bad7-423e-b701-a8fb6560bbd7","success":true,"total_likes":0}
```

* подключаемся к WebSocket

```bash
websocat -v ws://localhost:8000/post/feed/posted -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
```

* снова ставим лайк, убираем, ставим, ставим (убеждаемся, что saga_id меняется корректно)

```bash
curl -X POST http://localhost:9000/likes/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
{"message":"All SAGA steps completed successfully","saga_id":"like_saga_02129766-4dbf-418d-9c9a-a6176193114b","success":true,"total_likes":1}

curl -X DELETE http://localhost:9000/likes/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
{"message":"All SAGA steps completed successfully","saga_id":"like_saga_4e5190c9-ad42-4494-856a-210f630032cd","success":true,"total_likes":0}

curl -X POST http://localhost:9000/likes/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
{"message":"All SAGA steps completed successfully","saga_id":"like_saga_c10a6358-6f2c-4125-9052-b525482c801d","success":true,"total_likes":1}

curl -X POST http://localhost:9000/likes/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
{"message":"All SAGA steps completed successfully","saga_id":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb","success":true,"total_likes":2}
```

* пронаблюдаем за тем, что отображалось в клиенте WebSocket (убеждаемся, что обновления по счетчикам приходило корректно)

```bash
[INFO  websocat::lints] Auto-inserting the line mode
[INFO  websocat::stdio_threaded_peer] get_stdio_peer (threaded)
[INFO  websocat::ws_client_peer] get_ws_client_peer
[INFO  websocat::net_peer] Connected to TCP 127.0.0.1:8000
[INFO  websocat::ws_client_peer] Connected to ws
{"likes_count":1,"post_id":"4e104092-5fc5-4f29-a763-ce43300211ab","saga_id":"like_saga_02129766-4dbf-418d-9c9a-a6176193114b","timestamp":"1763118993232"}
[INFO  websocat::ws_peer] Received WebSocket ping
[INFO  websocat::ws_peer] Received WebSocket ping
{"likes_count":0,"post_id":"4e104092-5fc5-4f29-a763-ce43300211ab","saga_id":"like_saga_4e5190c9-ad42-4494-856a-210f630032cd","timestamp":"1763119063131"}
[INFO  websocat::ws_peer] Received WebSocket ping
{"likes_count":1,"post_id":"4e104092-5fc5-4f29-a763-ce43300211ab","saga_id":"like_saga_c10a6358-6f2c-4125-9052-b525482c801d","timestamp":"1763119078930"}
{"likes_count":2,"post_id":"4e104092-5fc5-4f29-a763-ce43300211ab","saga_id":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb","timestamp":"1763119083147"}
[INFO  websocat::ws_peer] Received WebSocket ping
```

* убеждаемся, что все записи по каждой SAGA попали в БД модуля **likes_service**

```bash
curl -X GET http://localhost:9000/likes/details/4e104092-5fc5-4f29-a763-ce43300211ab \
  -H "Authorization: Bearer b1fdfc3f-b531-40a9-8168-f65e94cec30c"
[
  {
    "id": 5,
    "op_type": "INCREMENT",
    "post_id": "4e104092-5fc5-4f29-a763-ce43300211ab",
    "saga_id": "like_saga_e79686e7-4844-4349-94ef-74ec36a7befb",
    "user_id": "b1fdfc3f-b531-40a9-8168-f65e94cec30c"
  },
  {
    "id": 4,
    "op_type": "INCREMENT",
    "post_id": "4e104092-5fc5-4f29-a763-ce43300211ab",
    "saga_id": "like_saga_c10a6358-6f2c-4125-9052-b525482c801d",
    "user_id": "b1fdfc3f-b531-40a9-8168-f65e94cec30c"
  },
  {
    "id": 3,
    "op_type": "DECREMENT",
    "post_id": "4e104092-5fc5-4f29-a763-ce43300211ab",
    "saga_id": "like_saga_4e5190c9-ad42-4494-856a-210f630032cd",
    "user_id": "b1fdfc3f-b531-40a9-8168-f65e94cec30c"
  },
  {
    "id": 2,
    "op_type": "INCREMENT",
    "post_id": "4e104092-5fc5-4f29-a763-ce43300211ab",
    "saga_id": "like_saga_02129766-4dbf-418d-9c9a-a6176193114b",
    "user_id": "b1fdfc3f-b531-40a9-8168-f65e94cec30c"
  },
  {
    "id": 1,
    "op_type": "DECREMENT",
    "post_id": "4e104092-5fc5-4f29-a763-ce43300211ab",
    "saga_id": "like_saga_f7c9bfa3-bad7-423e-b701-a8fb6560bbd7",
    "user_id": "b1fdfc3f-b531-40a9-8168-f65e94cec30c"
  }
]
```

* смотрим выводы логов распределенной между сервисами транзакции по одному из событий. в логах детально описываются все этапы, логируются Protobuf между сервисами, всё трассируется в рамках одной saga_id

```bash
likes_srv   | 2025-11-14 11:18:03.104383 [TRACE] (database.cpp:17) :: authenticate_user: query to MASTER tag='haproxy_lb:5000'
likes_srv   | 2025-11-14 11:18:03.105532 [DEBUG] (http_likes_controller.cpp:34) :: handler: POST /likes/:post_id
likes_srv   | 2025-11-14 11:18:03.105593 [INFOR] (likes_saga.cpp:41) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': starting execution for user 'b1fdfc3f-b531-40a9-8168-f65e94cec30c', post '4e104092-5fc5-4f29-a763-ce43300211ab'
likes_srv   | 2025-11-14 11:18:03.105605 [INFOR] (likes_saga.cpp:45) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': executing step 1 - SAVE_LIKE_INFO
likes_srv   | 2025-11-14 11:18:03.105614 [DEBUG] (likes_saga.cpp:216) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 1 - recording like in local DB
likes_srv   | 2025-11-14 11:18:03.105627 [TRACE] (database.cpp:486) :: update_like: query to MASTER tag='haproxy_lb:5000'
likes_srv   | 2025-11-14 11:18:03.120051 [DEBUG] (likes_saga.cpp:222) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 1 completed successfully
likes_srv   | 2025-11-14 11:18:03.120084 [INFOR] (likes_saga.cpp:45) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': executing step 2 - INCREMENT_LIKE_COUNT
likes_srv   | 2025-11-14 11:18:03.120091 [DEBUG] (likes_saga.cpp:294) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 2 - incrementing like count in social_service
likes_srv   | 2025-11-14 11:18:03.120116 [TRACE] (grpc_likes_client.cpp:57) :: increment_likes_handler: send Protobuf: {"postId":"4e104092-5fc5-4f29-a763-ce43300211ab","sagaId":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb"}
social_srv  | 2025-11-14 11:18:03.120741 [TRACE] (grpc_likes_service.cpp:50) :: IncrementLikeCountMessage: recv Protobuf: {"postId":"4e104092-5fc5-4f29-a763-ce43300211ab","sagaId":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb"}
social_srv  | 2025-11-14 11:18:03.120772 [TRACE] (app_database_service.cpp:367) :: inc_post_likes: query to MASTER tag='haproxy_lb:5000'
social_srv  | 2025-11-14 11:18:03.145065 [TRACE] (grpc_likes_service.cpp:62) :: IncrementLikeCountMessage: resp Protobuf: {"success":true,"newCount":2}
likes_srv   | 2025-11-14 11:18:03.145527 [TRACE] (grpc_likes_client.cpp:65) :: increment_likes_handler: recv Protobuf: {"success":true,"newCount":2}
likes_srv   | 2025-11-14 11:18:03.145595 [DEBUG] (likes_saga.cpp:305) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 2 completed successfully, new count 2
likes_srv   | 2025-11-14 11:18:03.145607 [INFOR] (likes_saga.cpp:45) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': executing step 3 - GET_AUTHOR_FRIENDS
likes_srv   | 2025-11-14 11:18:03.145613 [DEBUG] (likes_saga.cpp:384) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 3 - getting author's friends
likes_srv   | 2025-11-14 11:18:03.145629 [TRACE] (grpc_likes_client.cpp:117) :: get_author_friends_handler: send Protobuf: {"postId":"4e104092-5fc5-4f29-a763-ce43300211ab","sagaId":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb"}
social_srv  | 2025-11-14 11:18:03.145963 [TRACE] (grpc_likes_service.cpp:110) :: GetAuthorFriendsMessages: recv Protobuf: {"postId":"4e104092-5fc5-4f29-a763-ce43300211ab","sagaId":"like_saga_e79686e7-4844-4349-94ef-74ec36a7befb"}
social_srv  | 2025-11-14 11:18:03.145987 [TRACE] (app_database_service.cpp:458) :: get_post: query to REPLICA tag='haproxy_lb:5001'
social_srv  | 2025-11-14 11:18:03.146962 [TRACE] (app_database_service.cpp:243) :: get_friends: query to REPLICA tag='haproxy_lb:5001'
social_srv  | 2025-11-14 11:18:03.147481 [TRACE] (grpc_likes_service.cpp:128) :: GetAuthorFriendsMessages: resp Protobuf: {"friendIds":["b1fdfc3f-b531-40a9-8168-f65e94cec30c","3b287365-ac70-47e1-b9ba-40f56dc3dc7e"]}
likes_srv   | 2025-11-14 11:18:03.147688 [TRACE] (grpc_likes_client.cpp:125) :: get_author_friends_handler: recv Protobuf: {"friendIds":["b1fdfc3f-b531-40a9-8168-f65e94cec30c","3b287365-ac70-47e1-b9ba-40f56dc3dc7e"]}
likes_srv   | 2025-11-14 11:18:03.147725 [DEBUG] (likes_saga.cpp:395) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 3 completed successfully
likes_srv   | 2025-11-14 11:18:03.147734 [INFOR] (likes_saga.cpp:45) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': executing step 4 - NOTIFY_FRIENDS
likes_srv   | 2025-11-14 11:18:03.147740 [DEBUG] (likes_saga.cpp:415) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 4 - notifying friends via Kafka
likes_srv   | 2025-11-14 11:18:03.147769 [DEBUG] (kafka_likes_producer.cpp:80) :: KafkaLikesProducer produce: enqueued message (153 byte(s)) for topic 'user_b1fdfc3f-b531-40a9-8168-f65e94cec30c_post_likes'
likes_srv   | 2025-11-14 11:18:03.147780 [DEBUG] (kafka_likes_producer.cpp:18) :: KafkaDeliveryReporter: message delivered to topic 'user_b1fdfc3f-b531-40a9-8168-f65e94cec30c_post_likes' [9] at offset 1
likes_srv   | 2025-11-14 11:18:03.147787 [DEBUG] (kafka_likes_producer.cpp:18) :: KafkaDeliveryReporter: message delivered to topic 'user_3b287365-ac70-47e1-b9ba-40f56dc3dc7e_post_likes' [0] at offset 1
likes_srv   | 2025-11-14 11:18:03.147795 [DEBUG] (kafka_likes_producer.cpp:80) :: KafkaLikesProducer produce: enqueued message (153 byte(s)) for topic 'user_3b287365-ac70-47e1-b9ba-40f56dc3dc7e_post_likes'
likes_srv   | 2025-11-14 11:18:03.147799 [DEBUG] (likes_saga.cpp:433) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': step 4 completed successfully
likes_srv   | 2025-11-14 11:18:03.147802 [INFOR] (likes_saga.cpp:434) :: SAGA 'like_saga_e79686e7-4844-4349-94ef-74ec36a7befb': COMPLETED SUCCESSFULLY
ws_srv      | 2025-11-14 11:18:03.154273 [DEBUG] (kafka_client_consumer.cpp:177) :: KafkaConsumer thread: read message at offset 0, key: ??, timestamp: create time 1763119083147
```

### Выводы

* для ДЗ выбран сервис счетчиков лайков на посты
* разработан сервис счетчиков **likes_service**
* разработаны два сценария по паттерну SAGA на добавление лайка и удаление лайка
* сценарии SAGA охватывают 3 микросервиса, и технологии gRPC, Kafka, WebSocket, REST
* на примере убедились как SAGA обеспечивает консистентность распределенной транзакции между несколькими сервисами
* внедрили отображение счетчиков через сервис **ws_service** подключенным по WebSocket клиентам
* описаны SQL схемы таблиц и Protobuf
* система подготовлена к развертыванию в docker-compose
