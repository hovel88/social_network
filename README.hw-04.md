# Сервис социальной сети (курс Highload Architect)

## ДЗ 4: Лента постов и друзей (кеширование)

### Структура кеша

Библиотечное решение кеша предлагает на выбор несколько политик замены страниц кеша: Least Recently Used, Least Frequently Used, First-In/First-Out. в нашем сервисе будет использоваться LRU.

Для каждого пользователя будем хранить ленту из последних 1000 постов друзей (все вместе, отсортированные по времени создания).

Инвалидацию кеша будем делать комбинированным способом:

* Event-based, как наиболее подходящий для нашего случая вариант, для поддержания бОльшей актуальности

* TTL-based, как достаточно отказоустойчивый вариант, для защиты от "забытых" инвалидаций

В HTTP ответах на запросы будут добавлены следующие заголовки:

* **X-Pagination-Offset** - параметры "текущей страницы", значение поля offset из запроса /post/feed?offset=10&limit=10

* **X-Pagination-Limit** - параметры "текущей страницы", значение поля limit из запроса /post/feed?offset=10&limit=10

* **X-Cache-Status** - попадание в кеш (HIT) или отсутствие данных в кеше (MISS)

* **X-Cache-Total-Count** - общее количество доступных постов в кеше

* **X-Cache-Expires** - время, когда кеш протухнет (формат `2025-07-09T17:49:00Z`)

Сценарии:

* при добавлении нового поста - добавляем его в ленту всех друзей

* при удалении/обновлении поста - полная инвалидация кеша всех друзей (чтобы избежать неконсистентности)

* при удалении пользователя из друзей - удаляем посты из лент

* при чтении ленты - сначала проверяем в кеше, и если нет данных, то вытаскиваем из базы и актуализируем кеш (с добавлением TTL зля защиты от застрявших обновлений)


### Подготовка

* развернуть систему

```bash
docker compose -f docker-compose.hw-04.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.hw-04.yml down --remove-orphans
```

* скопировать в контейнер БД файл `misc/db_friends.sql`

```bash
docker cp ./misc/db_friends.sql postgres_db:/tmp/db_friends.sql
```

* применить файл к БД, это создаст новую таблицу **friends** с составным первичным ключом (user_id, friend_id), ограничением CHECK, для предотвращения дружбы с самим собой. также будут созданы два индекса для быстрого поиска друзей в обоих направлениях (**friends_user_id_btree_idx** и **friends_friend_id_btree_idx**)

```bash
docker exec -it postgres_db psql -U postgres -f /tmp/db_friends.sql
```

* скопировать в контейнер БД файл `misc/db_posts.sql`

```bash
docker cp ./misc/db_posts.sql postgres_db:/tmp/db_posts.sql
```

* применить файл к БД, это создаст новую таблицу **posts**. также будут созданы два индекса индексы для быстрого поиска постов по ID пользователя (/post/feed) и для сортировки (**posts_user_id_btree_idx** и **posts_created_at_btree_idx**)

```bash
docker exec -it postgres_db psql -U postgres -f /tmp/db_posts.sql
```

* т.к. ID пользователей (UUID) у нас генерируются на стороне БД, воспользуемся скриптом для генерации постов. тесто постов находится в файле `generator/posts.txt`, скрипт - `generator/generate_posts.py`.  
скрипт подключается к запущенной базе с подготовленными таблицами, считывает ID пользователей, для ускорения использует только 100 произвольных пользователей, и для каждого из них генерирует по 300 постов.

* по завершении работы скрипта-генератора можно проверить, какие пользователи были выбраны для написания постов (список UUID понадобится позже, чтобы формировать дружеские связи)

```bash
docker exec -it postgres_db psql -U postgres -c "
SELECT DISTINCT user_id FROM posts;"

               user_id                
--------------------------------------
 e27229af-ce60-4f7e-9df1-a3849dbeec63
 db7c147c-8ff6-4dae-a58f-5f65d0f1c636
 7d7ea051-2a42-4aa6-bac6-c46143abb02e
 1a3d4d8a-a519-4deb-a90c-2be08d08b155
 1881d7f9-96fb-458e-963e-f7a6bd06ef51
...
(100 rows)
```

### Проверка

* для примера, используем следующие ID пользователей, полученных ранее

```text
e27229af-ce60-4f7e-9df1-a3849dbeec63
db7c147c-8ff6-4dae-a58f-5f65d0f1c636
7d7ea051-2a42-4aa6-bac6-c46143abb02e
1a3d4d8a-a519-4deb-a90c-2be08d08b155
1881d7f9-96fb-458e-963e-f7a6bd06ef51
```

* запустим наблюдение логов сервиса

```bash
docker logs -f social_srv
```

* сейчас в базе нет дружеских связей, выполним запрос получения ленты

```bash
curl -v -X GET http://localhost:6000/post/feed \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Content-Type-Options: nosniff
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Date: Thu, 10 Jul 2025 09:26:33 GMT
< Keep-Alive: timeout=10, max=2
< X-Frame-Options: DENY
< Content-Length: 2
< X-Cache-Status: MISS
< Content-Type: application/json
< X-Pagination-Limit: 10
< X-Pagination-Offset: 0
< 
[]
```

убеждаемся, что список в ленте пуст (т.к. друзей нет), присутствуют заголовки в ответе:

* `X-Cache-Status: MISS`
* `X-Pagination-Limit: 10`
* `X-Pagination-Offset: 0`

в логах видим, что сервис сходил в базу 2 раза: (1) идентифицировать пользователя и (2) стянуть ленту друзей (но там ничего нет, т.к. друзей нет)

```bash
2025-07-10 09:26:33.667623 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#0, start processing task #0
2025-07-10 09:26:33.667763 [DEBUG] (app_post_service.cpp:65) :: handler: GET /post/feed
2025-07-10 09:26:33.669221 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 09:26:33.691557 [TRACE] (app_database_service.cpp:467) :: feed_post: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 09:26:33.731975 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:09:26:33 +0000] "GET /post/feed HTTP/1.1" 200 2 "-" "curl/7.68.0"
2025-07-10 09:26:33.732773 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#0, end processing task #0
```

* добавим пользователю **e27229af-ce60-4f7e-9df1-a3849dbeec63** `первого` друга **db7c147c-8ff6-4dae-a58f-5f65d0f1c636**

```bash
curl -X PUT http://localhost:6000/friend/set/db7c147c-8ff6-4dae-a58f-5f65d0f1c636 \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63" \
  -d ''
```

в логах видим, что сервис сходил в базу 2 раза: (1) идентифицировать пользователя и (2) добавить пользователя (в одной транзакции добавляется связь дружбы в обе стороны)

```bash
2025-07-10 11:29:54.726584 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#0, start processing task #1
2025-07-10 11:29:54.727030 [DEBUG] (app_friend_service.cpp:12) :: handler: PUT /friend/set/:id
2025-07-10 11:29:54.727684 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 11:29:54.734018 [TRACE] (app_database_service.cpp:220) :: add_friend: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 11:29:54.744559 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:11:29:54 +0000] "PUT /friend/set/db7c147c-8ff6-4dae-a58f-5f65d0f1c636 HTTP/1.1" 200 0 "-" "curl/7.68.0"
2025-07-10 11:29:54.745120 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#0, end processing task #1
```

* проверим, что после добавления друга, начали появляться посты в ленте

```bash
curl -v -X GET http://localhost:6000/post/feed \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Content-Type-Options: nosniff
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Date: Thu, 10 Jul 2025 12:32:47 GMT
< Keep-Alive: timeout=10, max=2
< X-Frame-Options: DENY
< Content-Length: 9224
< X-Cache-Status: MISS
< Content-Type: application/json
< X-Pagination-Limit: 10
< X-Pagination-Offset: 0
< 
[{"author_user_id":"db7c147c-8ff6-4dae-a58f-5f65d0f1c636","id":"b6ae2d00-e45c-4ef0-b082-905278251a9f","text":"Eget mi proin sed libero enim sed faucibus turpis. Tristique senectus et netus et. Tempus urna et pharetra pharetra massa massa. Viverra accumsan in nisl nisi scelerisque. Vitae sapien pellentesque habitant morbi tristique senectus et. Condimentum lacinia quis vel eros donec ac odio tempor orci. Lacus laoreet non curabitur gravida arcu ac tortor dignissim convallis. Lobortis elementum nibh tellus molestie nunc. Facilisi morbi tempus iaculis urna id. Elementum facilisis leo vel fringilla est ullamcorper eget nulla facilisi. Id semper risus in hendrerit gravida. Quam vulputate dignissim suspendisse in est. Magna eget est lorem ipsum. Leo a diam sollicitudin tempor id eu nisl nunc. Sed odio morbi quis commodo. Mollis nunc sed id semper risus in hendrerit gravida rutrum."}
...
```

убеждаемся, что список в ленте НЕ пуст (отображено 10 записей, в соответствии с настройками limit/offset), присутствуют заголовки в ответе:

* `X-Cache-Status: MISS`
* `X-Pagination-Limit: 10`
* `X-Pagination-Offset: 0`

заголовки показывают, что запрос не попал в кеш.  
в логах убеждаемся, что сервис сходил в базу 2 раза: (1) идентифицировать пользователя и (2) стянуть ленту друзей (т.к. в кеше данных нет)

```bash
2025-07-10 12:32:47.762016 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#1, start processing task #2
2025-07-10 12:32:47.762342 [DEBUG] (app_post_service.cpp:65) :: handler: GET /post/feed
2025-07-10 12:32:47.762406 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 12:32:47.768053 [TRACE] (app_database_service.cpp:467) :: feed_post: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 12:32:47.849462 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:12:32:47 +0000] "GET /post/feed HTTP/1.1" 200 9224 "-" "curl/7.68.0"
2025-07-10 12:32:47.849862 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#1, end processing task #2
```

* если сейчас постараться сделать повторный запрос быстро, то можно заметит, что повторный запрос выдал данные из кеша

```bash
curl -v -X GET http://localhost:6000/post/feed \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Pagination-Offset: 0
< X-Pagination-Limit: 10
< Content-Type: application/json
< Cache-Control: max-age=29, must-revalidate
< X-Content-Type-Options: nosniff
< X-Cache-Status: HIT
< Content-Length: 9224
< X-Frame-Options: DENY
< X-Cache-Total-Count: 300
< X-Cache-Expires: 2025-07-10T12:33:47Z
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Keep-Alive: timeout=10, max=2
< Date: Thu, 10 Jul 2025 12:33:18 GMT
< 
...
```

убеждаемся, что список в ленте НЕ пуст (отображено 10 записей, в соответствии с настройками limit/offset), присутствуют заголовки в ответе:

* `Cache-Control: max-age=29, must-revalidate`
* `X-Cache-Status: HIT`
* `X-Cache-Total-Count: 300`
* `X-Cache-Expires: 2025-07-10T12:33:47Z`
* `X-Pagination-Limit: 10`
* `X-Pagination-Offset: 0`

заголовки показывают, что запрос попал в кеш, в кеше сейчас 300 записей, кеш протухнет по TTL через 29 секунд (в 2025-07-10T12:33:47Z).  
в логах убеждаемся, что сервис сходил в базу 1 раз: (1) идентифицировать пользователя. за данными ленты сервис в базу не пошел, т.к. получил их из кеша

```bash
2025-07-10 12:33:18.784606 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#2, start processing task #3
2025-07-10 12:33:18.784868 [DEBUG] (app_post_service.cpp:65) :: handler: GET /post/feed
2025-07-10 12:33:18.784926 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 12:33:18.788379 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:12:33:18 +0000] "GET /post/feed HTTP/1.1" 200 9224 "-" "curl/7.68.0"
2025-07-10 12:33:18.789412 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#2, end processing task #3
```

* добавим пользователю **e27229af-ce60-4f7e-9df1-a3849dbeec63** `второго` друга **7d7ea051-2a42-4aa6-bac6-c46143abb02e**

```bash
curl -X PUT http://localhost:6000/friend/set/7d7ea051-2a42-4aa6-bac6-c46143abb02e \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63" \
  -d ''
```

* добавим пользователю **e27229af-ce60-4f7e-9df1-a3849dbeec63** `третьего` друга **1a3d4d8a-a519-4deb-a90c-2be08d08b155**

```bash
curl -X PUT http://localhost:6000/friend/set/1a3d4d8a-a519-4deb-a90c-2be08d08b155 \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63" \
  -d ''
```

* добавим пользователю **e27229af-ce60-4f7e-9df1-a3849dbeec63** `четвертого` друга **1881d7f9-96fb-458e-963e-f7a6bd06ef51**

```bash
curl -X PUT http://localhost:6000/friend/set/1881d7f9-96fb-458e-963e-f7a6bd06ef51 \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63" \
  -d ''
```

* далее запросим ленту постов друзей, в первый запрос, как и ожидалось, мы получим промах по кешу

```bash
curl -v -X GET http://localhost:6000/post/feed \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Content-Type-Options: nosniff
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Date: Thu, 10 Jul 2025 13:15:58 GMT
< Keep-Alive: timeout=10, max=2
< X-Frame-Options: DENY
< Content-Length: 9464
< X-Cache-Status: MISS
< Content-Type: application/json
< X-Pagination-Limit: 10
< X-Pagination-Offset: 0
< 
...
```

при этом по логам видим, что сервис сходил в БД и стянул записи постов

```bash
2025-07-10 13:15:58.733805 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#3, start processing task #15
2025-07-10 13:15:58.733989 [DEBUG] (app_post_service.cpp:65) :: handler: GET /post/feed
2025-07-10 13:15:58.734038 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 13:15:58.736634 [TRACE] (app_database_service.cpp:467) :: feed_post: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 13:15:58.774208 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:13:15:58 +0000] "GET /post/feed HTTP/1.1" 200 9464 "-" "curl/7.68.0"
2025-07-10 13:15:58.774703 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#3, end processing task #15
```

* сделаем повторный запрос

```bash
curl -v -X GET http://localhost:6000/post/feed \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Pagination-Offset: 0
< X-Pagination-Limit: 10
< Content-Type: application/json
< Cache-Control: max-age=54, must-revalidate
< X-Content-Type-Options: nosniff
< X-Cache-Status: HIT
< Content-Length: 9464
< X-Frame-Options: DENY
< X-Cache-Total-Count: 1000
< X-Cache-Expires: 2025-07-10T13:16:58Z
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Keep-Alive: timeout=10, max=2
< Date: Thu, 10 Jul 2025 13:16:04 GMT
< 
...
```

убеждаемся, что кеш прогрет и данные были получены из него, присутствуют заголовки в ответе:

* `Cache-Control: max-age=54, must-revalidate`
* `X-Cache-Status: HIT`
* `X-Cache-Total-Count: 1000`
* `X-Cache-Expires: 2025-07-10T13:16:58Z`
* `X-Pagination-Limit: 10`
* `X-Pagination-Offset: 0`

как видим, доступно в кеше **1000** записей, хотя мы добавили 4 друга, у каждого было сгенерировано по 300 сообщений, т.е. всего должно было быть 1200.  
по логам убеждаемся, что обращений в БД за списком постов не было

```bash
2025-07-10 13:16:04.445305 [TRACE] (thread_pool.cpp:58) :: thread HttpSrvPool#0, start processing task #16
2025-07-10 13:16:04.445576 [DEBUG] (app_post_service.cpp:65) :: handler: GET /post/feed
2025-07-10 13:16:04.445627 [TRACE] (app_database_service.cpp:19) :: authenticate_user: query to MASTER #0 tag='postgres_db:5432'
2025-07-10 13:16:04.450007 [TRACE] (app.cpp:321) :: 172.21.0.1 - - [10/Jul/2025:13:16:04 +0000] "GET /post/feed HTTP/1.1" 200 9464 "-" "curl/7.68.0"
2025-07-10 13:16:04.450988 [TRACE] (thread_pool.cpp:67) :: thread HttpSrvPool#0, end processing task #16
```

* убеждаемся, что происходит инвалидация кеша по TTL.  
для этого дожидаемся, пока истечет время (Cache-Control или X-Cache-Expires). повторяем запрос и видим, что в кеш мы не попали

```bash
curl -v -X GET http://localhost:6000/post/feed   -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Content-Type-Options: nosniff
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Date: Thu, 10 Jul 2025 13:25:08 GMT
< Keep-Alive: timeout=10, max=2
< X-Frame-Options: DENY
< Content-Length: 9464
< X-Cache-Status: MISS
< Content-Type: application/json
< X-Pagination-Limit: 10
< X-Pagination-Offset: 0
<
...
```

* убеждаемся, что при добавлении нового поста например `вторым` другом **7d7ea051-2a42-4aa6-bac6-c46143abb02e**, этот пост появится в кеше

* добавим свежий пост

```bash
curl -X POST http://localhost:6000/post/create \
  -H "Authorization: Bearer 7d7ea051-2a42-4aa6-bac6-c46143abb02e" \
  -d '{"text": "ололо трололо"}'

{"post_id":"235187f1-09ef-449a-b738-b31a824ec987"}
```

* а затем запросим ленту

```bash
curl -v -X GET 'http://localhost:6000/post/feed?offset=0&limit=2' \
  -H "Authorization: Bearer e27229af-ce60-4f7e-9df1-a3849dbeec63"

...
< HTTP/1.1 200 OK
< Content-Security-Policy: default-src 'self'
< X-Pagination-Offset: 0
< X-Pagination-Limit: 2
< Content-Type: application/json
< Cache-Control: max-age=1, must-revalidate
< X-Content-Type-Options: nosniff
< X-Cache-Status: HIT
< Content-Length: 1027
< X-Frame-Options: DENY
< X-Cache-Total-Count: 1000
< X-Cache-Expires: 2025-07-10T13:34:00Z
< Server: social_network/1.0 (Linux) httplib/0.20.0
< Keep-Alive: timeout=10, max=2
< Date: Thu, 10 Jul 2025 13:33:59 GMT
< 
[{"author_user_id":"7d7ea051-2a42-4aa6-bac6-c46143abb02e","id":"235187f1-09ef-449a-b738-b31a824ec987","text":"ололо трололо"},{"author_user_id":"db7c147c-8ff6-4dae-a58f-5f65d0f1c636","id":"b6ae2d00-e45c-4ef0-b082-905278251a9f","text":"Eget mi proin sed libero enim sed faucibus turpis. Tristique senectus et netus et. Tempus urna et pharetra pharetra massa massa. Viverra accumsan in nisl nisi scelerisque. Vitae sapien pellentesque habitant morbi tristique senectus et. Condimentum lacinia quis vel eros donec ac odio tempor orci. Lacus laoreet non curabitur gravida arcu ac tortor dignissim convallis. Lobortis elementum nibh tellus molestie nunc. Facilisi morbi tempus iaculis urna id. Elementum facilisis leo vel fringilla est ullamcorper eget nulla facilisi. Id semper risus in hendrerit gravida. Quam vulputate dignissim suspendisse in est. Magna eget est lorem ipsum. Leo a diam sollicitudin tempor id eu nisl nunc. Sed odio morbi quis commodo. Mollis nunc sed id semper risus in hendrerit gravida rutrum.
"}]
```

* как видим по заголовкам, мы вытащили данные из кеша, в нём всё еще 1000 записей (хотя мы только добавили дополнительный пост), и в соотвествии с настройками offset/limit было выдано 2 записи.  
первая из них - наш только что добавленный пост от друга, эта первая запись в выдаче, потмоу что лента отсортирована по дате создания поста.

```json
{"author_user_id":"7d7ea051-2a42-4aa6-bac6-c46143abb02e","id":"235187f1-09ef-449a-b738-b31a824ec987","text":"ололо трололо"}
```
