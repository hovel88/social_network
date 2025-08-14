# Сервис социальной сети (курс Highload Architect)

## ДЗ 7: Применение In-Memory СУБД (работа с UDF)

Для проверки работы с UDF, будем применять Tarantool. Он позволяет написать на Lua полноценные скрипты, внутри которых можно обращаться к внешним источникам данных.

Ранее был реализован сервис диалогов, применялся он в изучении шардирования. Переиспользуем, однако не будет применять шардирование, базу PostgreSQL запустим в режиме одного узла мастера.

Для разворачивания системы используется docker-compose файл:

* `docker-compose.service-inmemory.yml`

Тут Docker развернет наш сервис **social_srv**, один инстанс PostgreSQL **postgres_db** и один инстанс Tarantool **tarantool_db**.

Образ Tarantool используется не чистой версии **tarantool/tarantool:3.3.2**, т.к. в нем отсутствует модуль **pg**, а он нам потребуется для подключения к базе из скрипта. Поэтому образ Tarantool собирается кастомный, в файле `tarantool/Dockerfile`, в котором производится дополнительная установка Lua-модуля **pg**.

Исходный код сервиса претерпел изменения:

* механизм аутентификации пользователя теперь перенесен в вызов удаленной функции `check_user` в Tarantool. Ранее сервис напрямую обращался к PostgreSQL.
* отправка нового диалогового сообщения теперь реализована через вызов удаленной функции `send_dialog_message` в Tarantool. Ранее сервис напрямую обращался к PostgreSQL.
* получение диалога теперь реализована через вызов удаленной функции `list_dialog_messages` в Tarantool. Ранее сервис напрямую обращался к PostgreSQL.

Скрипт с UDF на языке Lua находится тут:

* `tarantool/scripts/udf.lua`

Основная задача - вести своего рода сквозной кеш. Т.е. в Lua создаются таблицы (space) соответственно **user_cache** и **dialog_cache**. Первая используется для аутентификации пользователя, в ней хранится UUID и хэш пароля пользователя. Вторая используется для системы диалогов, этот space повторяет схему таблицы из PostgreSQL.

Также создаются индексы, чтобы оперировать таблицей. И в довершение - создается пользователь **tntuser**/**tntpass**, которому разрешено запускать на исполнение функции.

В общем-то логика UDF-функций аутентификации проста:

1. скрипт получает запрос **check_user** с параметрами вызова
2. проверяет есть ли в кеше пользователь с таким UUID, и если его нет - то обращается к PostgreSQL с соответствующим SELECT вызовом
3. если из БД получены данные, то обновленные записи скрипт помещает в свой кеш, и в следующий раз этап (2) будет пропущен
4. ID с хешем возвращаются в ответ на запрос, что символизирует, что такой пользователь присутствует в системе

Логика UDF-функций отправки диалогового сообщения:

1. скрипт получает запрос **send_dialog_message** с параметрами вызова
2. из параметров вызова формируется запись и помещается в space, таким образом в кеше всегда актуальные данные
3. в фоне (в fiber) создается задача, чтобы эти данные через запрос INSERT поместить в таблицу в PostgreSQL
4. актуальные из БД поля **dialog_id** и **created_at** после вставки обновляются в записи в space, чтобы наш кеш был в полном соответствии с таблицей PostgreSQL
5. ответ возвращается пользователю сразу, вне зависимости от выполнения фоновой задачи (этапы (3) и (4))

Логика UDF-функций получения диалога:

1. скрипт получает запрос **list_dialog_messages** с параметрами вызова
2. собираем из space подходящие записи (по полю shard_key (который в формате `UUIDfrom_UUIDto`))
3. если данных в кеше недостаточно, то формируется SELECT в таблицу в PostgreSQL
4. полученные данные добавляются в space, таким образом для последующего запроса обращаться к PostgreSQL не потребуется
5. полученные данные объединяются с извлеченными ранее из кеша записями
6. набор сообщений сортируется по полю `created_at` и возвращается пользователю

Таким образом, реализуя UDF в виде Lua-скрипта, мы получаем In-Memory базу Tarantool, которую используем в функции промежуточного кеша, и в эту самую UDF перемещаем логику, для обновления источника истины - базы PostgreSQL. Наш сервис потенциально становится легче, отдав часть логики. Однако из минусов - теперь эта часть логики нам стала не подконтрольна!

### Описание нагрузочного теста на запись

* в качестве нагрузки на запись используется K6 тест из файла `k6_tests/dialogs_send_and_list.js`

* схема нагрузки:
  * в первую минуту - 10 клиентов  
  * далее в течении 3 минут - 200 клиентов
  * в последнюю минуту - нагрузка снижается до 10 клиентов

* на первоначальном этапе (`setup()`) формируются дополнительный набор сообщений между пользователями
  * вызов эндпоинта `/dialog/${user_id}/send`
  * сообщения формата `Message ${j} from ${user_from} to ${user_to}`

* выполняется нагрузка чередование по двум эндпойнтам:
  * `/dialog/${user_id}/send`
  * `/dialog/${user_id}/list`

### Подготовка

* развернуть систему

```bash
docker compose -f docker-compose.service-inmemory.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.service-inmemory.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml down --remove-orphans
```

* убедиться, что таблица `dialogs` существует, для этого
  * скопировать в контейнер БД файл `misc/db_dialogs.sql`

  ```bash
  docker cp ./misc/db_dialogs.sql postgres_db:/tmp/db_dialogs.sql
  ```

  * применить файл к БД, это создаст новую таблицу **dialogs**. также будут созданы два индекса для быстрого поиска диалогов от одного к другому пользователю и для поля ключа шардирвоания (**dialogs_from_to_btree_idx**, **dialogs_to_from_btree_idx** и **dialogs_shard_key_btree_idx**)

  ```bash
  docker exec -it postgres_db psql -U postgres -f /tmp/db_dialogs.sql
  ```

* затем получим несколько UUID пользователей, с которыми будем работать далее (именно эти UUID используются в нагрузочном тесте)

```bash
docker exec -it postgres_db psql -U postgres -c "
SELECT id FROM users LIMIT 6;"

                  id                  
--------------------------------------
 2f8d9273-3fb3-48ef-b98b-310dacf79316
 dc3e666d-97cd-46cf-b151-d942022f435e
 e5aedd63-6105-4174-a75c-30fdd511fe15
 cf093a1f-2e19-41b6-bcb0-db7d4db793ba
 f776b179-828d-47d9-937d-66ab8a9de5b2
 d6f778e1-c685-4393-8a7b-abef8596e3e5
(6 rows)
```

### Проверка

* сначала разворачиваем систему, в которой наш сервис более старой версии, т.е. не использует Tarantool и напрямую обращается в PostgreSQL.  
для этого в файле `.env` нужно указать `ENV_SOCIAL_SERVICE_IMAGE=social_network:5`  
**ПРИМЕЧАНИЕ:** важно, чтобы образ уже был собран ранее

* запускаем нагрузочный тест командой

```bash
docker compose -f docker-compose.service-inmemory.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml run k6 run --verbose --out experimental-prometheus-rw --http-debug="full" /tests/dialogs_send_and_list.js
```

* собираем результаты теста

* затем разворачиваем систему с обновленным сервисом социальной сети, т.е. использующим Tarantool, вместо прямого обращения в PostgreSQL.  
для этого в файле `.env` нужно указать `ENV_SOCIAL_SERVICE_IMAGE=social_network:6`

* запускаем нагрузочный тест командой

```bash
docker compose -f docker-compose.service-inmemory.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml run k6 run --verbose --out experimental-prometheus-rw --http-debug="full" /tests/dialogs_send_and_list.js
```

* в логах сервиса социальной сети можно видеть как чередуются запросы

```text
2025-08-05 10:51:49.169871 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "POST /dialog/dc3e666d-97cd-46cf-b151-d942022f435e/send HTTP/1.1" OK 0 "-" "Grafana k6/1.1.0"
2025-08-05 10:51:49.198822 [DEBUG] (app_dialog_service.cpp:13) :: handler: POST /dialog/:id/send
2025-08-05 10:51:49.199737 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "POST /dialog/cf093a1f-2e19-41b6-bcb0-db7d4db793ba/send HTTP/1.1" OK 0 "-" "Grafana k6/1.1.0"
2025-08-05 10:51:49.211956 [DEBUG] (app_dialog_service.cpp:13) :: handler: POST /dialog/:id/send
2025-08-05 10:51:49.212781 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "POST /dialog/d6f778e1-c685-4393-8a7b-abef8596e3e5/send HTTP/1.1" OK 0 "-" "Grafana k6/1.1.0"
2025-08-05 10:51:49.372001 [DEBUG] (app_dialog_service.cpp:29) :: handler: GET /dialog/:id/list
2025-08-05 10:51:49.374003 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "GET /dialog/2f8d9273-3fb3-48ef-b98b-310dacf79316/list HTTP/1.1" OK 19797 "-" "Grafana k6/1.1.0"
2025-08-05 10:51:49.400743 [DEBUG] (app_dialog_service.cpp:29) :: handler: GET /dialog/:id/list
2025-08-05 10:51:49.402755 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "GET /dialog/e5aedd63-6105-4174-a75c-30fdd511fe15/list HTTP/1.1" OK 19779 "-" "Grafana k6/1.1.0"
2025-08-05 10:51:49.414723 [DEBUG] (app_dialog_service.cpp:29) :: handler: GET /dialog/:id/list
2025-08-05 10:51:49.416987 [TRACE] (app.cpp:272) :: 172.21.0.9 - - [05/Aug/2025:10:51:49 +0000] "GET /dialog/f776b179-828d-47d9-937d-66ab8a9de5b2/list HTTP/1.1" OK 19761 "-" "Grafana k6/1.1.0"
```

* в логах Tarantool можно видеть как обрабатываются запросы UDF

```text
2025-08-05 10:52:00.900 [1] main/867/main/udf udf.lua:109 I> send_dialog_message(): from=f776b179-828d-47d9-937d-66ab8a9de5b2  to=d6f778e1-c685-4393-8a7b-abef8596e3e5
2025-08-05 10:52:00.901 [1] main/1634/lua/udf udf.lua:127 I> send_dialog_message(): query=INSERT INTO dialogs (from_user_id, to_user_id, message, shard_key)
VALUES ('f776b179-828d-47d9-937d-66ab8a9de5b2', 'd6f778e1-c685-4393-8a7b-abef8596e3e5', 'New message at 1754391120899 from f776b179-828d-47d9-937d-66ab8a9de5b2 to d6f778e1-c685-4393-8a7b-abef8596e3e5', 'f776b179-828d-47d9-937d-66ab8a9de5b2_d6f778e1-c685-4393-8a7b-abef8596e3e5')
RETURNING dialog_id::text, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms

2025-08-05 10:52:00.909 [1] main/1632/lua/udf udf.lua:135 I> send_dialog_message(): dialog updated (id: e7d3c38d-cb78-4f34-9fc8-80b42b56da78 -> d80e91d4-4952-4b53-8e17-0e3ef24f9535, created_at: 1754391120000 -> 1754391120900)
2025-08-05 10:52:00.917 [1] main/867/main/udf udf.lua:41 I> check_user(): uuid=2f8d9273-3fb3-48ef-b98b-310dacf79316
2025-08-05 10:52:00.917 [1] main/867/main/udf udf.lua:44 I> {"pwd_hash":"$2a$12$XH4BS2zgGgpJs4hiu9p17OwxHxoWto21DLHkzo6JQH67U/3wi.LEW","id":"2f8d9273-3fb3-48ef-b98b-310dacf79316"}
2025-08-05 10:52:00.918 [1] main/867/main/udf udf.lua:109 I> send_dialog_message(): from=2f8d9273-3fb3-48ef-b98b-310dacf79316  to=dc3e666d-97cd-46cf-b151-d942022f435e
2025-08-05 10:52:00.918 [1] main/1635/lua/udf udf.lua:127 I> send_dialog_message(): query=INSERT INTO dialogs (from_user_id, to_user_id, message, shard_key)
VALUES ('2f8d9273-3fb3-48ef-b98b-310dacf79316', 'dc3e666d-97cd-46cf-b151-d942022f435e', 'New message at 1754391120912 from 2f8d9273-3fb3-48ef-b98b-310dacf79316 to dc3e666d-97cd-46cf-b151-d942022f435e', '2f8d9273-3fb3-48ef-b98b-310dacf79316_dc3e666d-97cd-46cf-b151-d942022f435e')
RETURNING dialog_id::text, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms

2025-08-05 10:52:00.926 [1] main/1634/lua/udf udf.lua:135 I> send_dialog_message(): dialog updated (id: 605e2711-e350-4a0e-8b9f-3b388d54f8ee -> f3a01af5-049e-4f38-8be5-ba08cc146aae, created_at: 1754391120000 -> 1754391120919)
2025-08-05 10:52:00.926 [1] main/1633/lua/udf udf.lua:135 I> send_dialog_message(): dialog updated (id: 521dcd35-8d1e-498b-93af-61e211a89640 -> bee76d8a-2f42-48ea-9749-b11a1bb05cf5, created_at: 1754391120000 -> 1754391120917)
2025-08-05 10:52:00.931 [1] main/1635/lua/udf udf.lua:135 I> send_dialog_message(): dialog updated (id: e583389e-8315-4c6a-9713-7166f905e6fa -> 662e241d-427d-42b6-9761-c9cad2c33d56, created_at: 1754391120000 -> 1754391120928)
2025-08-05 10:52:01.051 [1] main/867/main/udf udf.lua:41 I> check_user(): uuid=dc3e666d-97cd-46cf-b151-d942022f435e
2025-08-05 10:52:01.051 [1] main/867/main/udf udf.lua:44 I> {"pwd_hash":"$2a$12$XH4BS2zgGgpJs4hiu9p17OwxHxoWto21DLHkzo6JQH67U/3wi.LEW","id":"dc3e666d-97cd-46cf-b151-d942022f435e"}
2025-08-05 10:52:01.051 [1] main/867/main/udf udf.lua:162 I> list_dialog_messages(): from=dc3e666d-97cd-46cf-b151-d942022f435e  to=2f8d9273-3fb3-48ef-b98b-310dacf79316  limit=100
2025-08-05 10:52:01.056 [1] main/867/main/udf udf.lua:180 I> list_dialog_messages(): cached 705 element(s)
2025-08-05 10:52:01.086 [1] main/867/main/udf udf.lua:41 I> check_user(): uuid=cf093a1f-2e19-41b6-bcb0-db7d4db793ba
2025-08-05 10:52:01.087 [1] main/867/main/udf udf.lua:44 I> {"pwd_hash":"$2a$12$XH4BS2zgGgpJs4hiu9p17OwxHxoWto21DLHkzo6JQH67U/3wi.LEW","id":"cf093a1f-2e19-41b6-bcb0-db7d4db793ba"}
2025-08-05 10:52:01.087 [1] main/867/main/udf udf.lua:162 I> list_dialog_messages(): from=cf093a1f-2e19-41b6-bcb0-db7d4db793ba  to=e5aedd63-6105-4174-a75c-30fdd511fe15  limit=100
2025-08-05 10:52:01.091 [1] main/867/main/udf udf.lua:180 I> list_dialog_messages(): cached 700 element(s)
```

* собираем результаты теста

### Анализ результатов

* результат ДО

```text
DEBU[0404] Generating the end-of-test summary...        
     █ Send message

       ✓ send status 200

     █ List messages

       ✓ list status 200

     checks.........................: 100.00% ✓ 2028     ✗ 0    
     data_received..................: 22 MB   54 kB/s
     data_sent......................: 810 kB  2.0 kB/s
     group_duration.................: avg=8.62s    min=15.19ms med=50.82ms  max=1m48s   p(90)=30.78s   p(95)=35.64s  
     http_req_blocked...............: avg=240µs    min=2.43µs  med=124.12µs max=23.91ms p(90)=528.41µs p(95)=603.89µs
     http_req_connecting............: avg=186.81µs min=0s      med=93.69µs  max=23.86ms p(90)=396.78µs p(95)=456.41µs
     http_req_duration..............: avg=6.66s    min=1.94ms  med=42.73ms  max=1m48s   p(90)=27.9s    p(95)=34.2s   
       { expected_response:true }...: avg=6.66s    min=1.94ms  med=42.73ms  max=1m48s   p(90)=27.9s    p(95)=34.2s   
     ✓ { type:list }................: avg=35.92ms  min=15.02ms med=39.31ms  max=82.55ms p(90)=45.98ms  p(95)=46.99ms 
     ✗ { type:send }................: avg=17.15s   min=18.76ms med=15.82s   max=1m48s   p(90)=35.64s   p(95)=38.09s  
     http_req_failed................: 0.00%   ✓ 0        ✗ 2628 
     ✓ { type:list }................: 0.00%   ✓ 0        ✗ 1010 
     ✓ { type:send }................: 0.00%   ✓ 0        ✗ 1018 
     http_req_receiving.............: avg=336.85µs min=67.33µs med=290.41µs max=10.77ms p(90)=598.74µs p(95)=702.26µs
     http_req_sending...............: avg=54.47µs  min=5.17µs  med=37.68µs  max=1.49ms  p(90)=105.12µs p(95)=122.94µs
     http_req_tls_handshaking.......: avg=0s       min=0s      med=0s       max=0s      p(90)=0s       p(95)=0s      
     http_req_waiting...............: avg=6.66s    min=1.82ms  med=42.14ms  max=1m48s   p(90)=27.9s    p(95)=34.2s   
     ✓ { type:list }................: avg=35.33ms  min=14.77ms med=38.74ms  max=71.74ms p(90)=45.39ms  p(95)=46.38ms 
     ✗ { type:send }................: avg=17.15s   min=18.65ms med=15.82s   max=1m48s   p(90)=35.64s   p(95)=38.09s  
     http_reqs......................: 2628    6.517891/s
     iteration_duration.............: avg=8.82s    min=215.7ms med=251.3ms  max=1m48s   p(90)=30.98s   p(95)=35.84s  
     iterations.....................: 2028    5.029788/s
     vus............................: 10      min=0      max=200

running (6m43.2s), 000/200 VUs, 2028 complete and 191 interrupted iterations
default ✓ [======================================] 010/200 VUs  5m0s
```

* результат ПОСЛЕ

```text
DEBU[0363] Generating the end-of-test summary...        
     █ Send message

       ✓ send status 200

     █ List messages

       ✓ list status 200

     checks.........................: 100.00% ✓ 17678     ✗ 0    
     data_received..................: 188 MB  520 kB/s
     data_sent......................: 4.9 MB  14 kB/s
     group_duration.................: avg=1.23s    min=1.09ms   med=1.06s    max=1m4s    p(90)=2.12s    p(95)=2.4s    
     http_req_blocked...............: avg=9.13µs   min=1.58µs   med=3.16µs   max=31.26ms p(90)=4.91µs   p(95)=8.26µs  
     http_req_connecting............: avg=4.97µs   min=0s       med=0s       max=31.23ms p(90)=0s       p(95)=0s      
     http_req_duration..............: avg=1.19s    min=868.04µs med=1.02s    max=1m4s    p(90)=2.11s    p(95)=2.38s   
       { expected_response:true }...: avg=1.19s    min=868.04µs med=1.02s    max=1m4s    p(90)=2.11s    p(95)=2.38s   
     ✗ { type:list }................: avg=1.12s    min=11.27ms  med=1.07s    max=4.67s   p(90)=2.12s    p(95)=2.36s   
     ✗ { type:send }................: avg=1.34s    min=868.04µs med=1.05s    max=1m4s    p(90)=2.12s    p(95)=2.63s   
     http_req_failed................: 0.00%   ✓ 0         ✗ 18278
     ✓ { type:list }................: 0.00%   ✓ 0         ✗ 8791 
     ✓ { type:send }................: 0.00%   ✓ 0         ✗ 8887 
     http_req_receiving.............: avg=342.16µs min=37.73µs  med=267.77µs max=19.33ms p(90)=588.44µs p(95)=792.47µs
     http_req_sending...............: avg=11.37µs  min=4.26µs   med=8.67µs   max=1.93ms  p(90)=14.84µs  p(95)=24.96µs 
     http_req_tls_handshaking.......: avg=0s       min=0s       med=0s       max=0s      p(90)=0s       p(95)=0s      
     http_req_waiting...............: avg=1.19s    min=792.55µs med=1.02s    max=1m4s    p(90)=2.11s    p(95)=2.38s   
     ✗ { type:list }................: avg=1.12s    min=10.9ms   med=1.07s    max=4.67s   p(90)=2.12s    p(95)=2.36s   
     ✗ { type:send }................: avg=1.34s    min=792.55µs med=1.05s    max=1m4s    p(90)=2.12s    p(95)=2.63s   
     http_reqs......................: 18278   50.422438/s
     iteration_duration.............: avg=1.43s    min=201.19ms med=1.26s    max=1m4s    p(90)=2.32s    p(95)=2.6s    
     iterations.....................: 17678   48.767253/s
     vus............................: 13      min=0       max=200

running (6m02.5s), 000/200 VUs, 17678 complete and 28 interrupted iterations
default ✓ [======================================] 000/200 VUs  5m0s
```

Получаем очень интересный результат.

* результат ДО:
  * нагрузочный тест завершился успешно
  * пропускная способность: **810 kB** отправлено и **22 MB** получено
  * отработало всего **2028** итераций (list: 1010, send: 1018)
  * 95-перцентиль длительности итерации: **35.84s**
  * latency вызова эндпоинта `list`: avg=35.92ms  min=15.02ms med=39.31ms  max=82.55ms p(90)=45.98ms  p(95)=46.99ms
  * latency вызова эндпоинта `send`: avg=17.15s   min=18.76ms med=15.82s   max=1m48s   p(90)=35.64s   p(95)=38.09s

* результат ПОСЛЕ:
  * (=) нагрузочный тест завершился успешно
  * (+) пропускная способность: **4.9 MB** отправлено и **188 MB** получено
  * (+) отработало **17678** итераций (list: 8791, send: 8887)
  * (+) 95-перцентиль длительности итерации: **2.6s**
  * (-) latency вызова эндпоинта `list`: avg=1.12s    min=11.27ms  med=1.07s    max=4.67s   p(90)=2.12s    p(95)=2.36s
  * (+) latency вызова эндпоинта `send`: avg=1.34s    min=868.04µs med=1.05s    max=1m4s    p(90)=2.12s    p(95)=2.63s

Считаю, что значительное влияние в обработке эндпоинта `send` оказал переход к асинхронному взаимодействию (т.к. раньше сервис с записью шел в PostgreSQL и это был блокирующий вызов, а теперь для записи в PostgreSQL используется фоновая задача (fiber), в то время как сервис получает ответ  сразу же после помещения данных в in-memory space).  
Однако видна просадка в обработке эндпоинта `list` (думаю, что PostgreSQL значительно более оптимизирован, в то время как в Tarantool UDF процедуру писал я и она получилась не самая лучшая, с объединением результатов кеша и ответа на запрос SELECT из PostgreSQL).  
В целом, использование Tarantool в роли in-memory кеша (своего рода сквозной кеш получился), позволило получить значительный прирост в пропускной способности, и вынести часть логики из сервиса.
