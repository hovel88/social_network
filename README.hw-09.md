# Сервис социальной сети (курс Highload Architect)

## ДЗ 9: Отказоустойчивость приложений

### Описание

По постановке ДЗ, требуется развернуть отказоустойчивую высокодоступную систему.  

Распространенной реализацией принципа высокой доступности (HA - high availability) в среде PostgreSQL является использование прокси-сервера: вместо прямого подключения к серверу базы данных приложение подключается к прокси-серверу, который перенаправляет запрос к PostgreSQL. А за прокси-сервером располагается кластер из нескольких машин с БД, в котором настроена репликация, чтобы поломка отдельного узла (или нескольких) не привела к потере данных, а также сохранился доступ к PostgreSQL со стороны микросервисов.

В качестве прокси-сервера (и балансировщика нагрузки) хорошо подходит HAproxy. При использовании HAproxy можно также направлять запросы на чтение в одну или несколько реплик для балансировки нагрузки. Однако, приложение должно это учитывать и самостоятельно разделять трафик на чтение и запись. С HAproxy это достигается путем предоставления приложению двух разных портов для подключения. Для этого произведена небольшая доработка в сервисе **social_network**: исправлена логика пула соединений до БД, теперь сервис знает только единые точки входа в мастер и реплику, а балансировка делается внешняя.

Посмотреть конфигурацию HAproxy можно в файле `load_balancing/haproxy/haproxy.cfg`.  

В конфигурации есть две секции: `postgres-master` (доступ RW), использующая порт **5000**, и `postgres-replicas` (доступ RO), использующая порт **5001**. Все три узла включены в обе секции: это связано с тем, что они являются потенциальными кандидатами на роль основного или дублирующего узла. Для того чтобы HAproxy узнал, какова роль каждого узла в данный момент, он посылает HTTP-запрос на порт 8008 узла, а Patroni ответит на него, сообщив является ли узел мастером и репликой.

Ранее экземплярами БД PostgreSQL у нас выступали контейнеры из образа **postgres:16-alpine**. Но для работы системы нам потребуется подготовить образ, добавив в него систему управления Patroni. Для этого подготовлен специальный Dockerfile (вот тут `load_balancing/patroni/Dockerfile`), который к образу **postgres:16-alpine** добавляет необходимые библиотеки и систему Patroni. Также в новом образе выключен запуск postgresql (переопределены entrypoint и cmd). За запуск и управление postgresql теперь отвечает Patroni.

Конфигурация для Patroni монтируется в контейнер и с ней запускаются экземпляры кластера, ее можно посмотреть в файлах:

* `load_balancing/patroni/patroni_0.yml`
* `load_balancing/patroni/patroni_1.yml`
* `load_balancing/patroni/patroni_2.yml`

Чтобы кластер смог работать и Patroni мог корректно выбирать мастер/реплики, ему потребуется отдельный кластер из узлов ETCD (распределенное хранилище данных типа «ключ-значение»). В системе запускается такой кластер, состоящий из трех инстансов.

Чтобы обеспечить высокодоступную и отказоустойчивую систему микросервисов, также применяется подход с прокси-сервером (балансировщиком), за котором располагаются несколько (в нашем случае два) экземпляра сервиса **social_network**. Таким образом, клиенты всегда обращаются через единую точку входа в виде адреса балансировщика, не знаю ничего о внутренней структуре сервисов. В тоже время, выход из строя микросервиса не приведет к потере доступности. В качестве балансировщика используется Nginx.

Посмотреть конфигурацию Nginx можно в файле `load_balancing/nginx/nginx.conf`.

**ПРИМЕЧАНИЕ 1:** Если бы к системе распределенной БД подключалось очень много сервисов (десятки-сотни), да еще и в виде нескольких инстансов (единицы-десятки), и каждый из которых открывал множество соединений (десятки) к базе. То для повышения доступности имело бы смысл добавить в систему еще элемент **PgBouncer**, представляющий пулер соединений к базе, чтобы во-первых, не перегрузить соединениями базу и в пиках нагрузки - ставить клиентские запросы в очередь на подключения, а во-вторых, не открывать/закрывать постоянно новые соединения на базе (т.к. это дорогой процесс), а отдавать соединения в аренду пользователям.  
PgBouncer можно было бы расположить сразу после HAproxy и до каждого инстанса Patroni+PostgreSQL. Балансировку HAproxy в этом случае будет производить на PgBouncer, а тот уже отвечает за пул соединений до своего подопечного экземпляра Patroni+PostgreSQL.  
Один PgBouncer на все экземпляры Patroni+PostgreSQL делать наверное не очень хорошо, т.к. он однопоточный и может стать узким горлышком, да и запущенный в виде нескольких экземплярах он не станет единой точкой отказа.

**ПРИМЕЧАНИЕ 2:** Также для повышения надежности можно задублировать балансировщики, добавить второй экземпляр в виде Hot Standby, чтобы они не были единой точкой отказа. И на каждом инстансе балансировщика разместить элемент **Keepalived**, который бы управлял назначением виртуального IP адреса (VIP) на один основной экземпляр, а при сбое - быстро вводил бы в работу запасной экземпляр балансировщика. Но при этом разворачивать систему надо на отдельных хостах, а не в Docker Compose.

### Предварительная подготовка

* собрать новую версию сервиса **social_network**, для этого выполнить скрипт `build.social_network.sh` в корне репозитория.  
В результате, с помощью образа **alpine-cpp-builder:2** будет произведена компиляция бинарника и генерация Docker-файла в каталоге `service/_build`, а далее выполнится сборка образа сервиса **social_network:9**

### Подготовка

Развернуть систему:

```bash
docker compose -f docker-compose.hw-09.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.hw-09.yml down --remove-orphans
```

В результате будет развернут ряд сервисов, среди них:

* `social_network:9`
* `haproxy:2.4.29-alpine`
* `nginx:1.28.0-alpine`
* `etcd:v3.5.15`
* `pg_patroni` (наш собственный образ PostgreSQL + Patroni)

Убедиться, что кластер ETCD запущен:

```bash
docker exec etcd0 etcdctl member list --write-out="table"
+------------------+---------+-------+-------------------+-------------------+------------+
|        ID        | STATUS  | NAME  |    PEER ADDRS     |   CLIENT ADDRS    | IS LEARNER |
+------------------+---------+-------+-------------------+-------------------+------------+
| ade526d28b1f92f7 | started | etcd1 | http://etcd1:2380 | http://etcd1:2379 |      false |
| cf1d15c5d194b5c9 | started | etcd0 | http://etcd0:2380 | http://etcd0:2379 |      false |
| d282ac2ce600c1ce | started | etcd2 | http://etcd2:2380 | http://etcd2:2379 |      false |
+------------------+---------+-------+-------------------+-------------------+------------+
```

Определим, кто из инстансов кластера ETCD является лидером (лидером оказался **etcd2**):

```bash
docker exec etcd0 etcdctl endpoint status --endpoints http://etcd0:2380 --write-out="table"
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
|     ENDPOINT      |        ID        | VERSION | DB SIZE | IS LEADER | IS LEARNER | RAFT TERM | RAFT INDEX | RAFT APPLIED INDEX | ERRORS |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
| http://etcd0:2380 | cf1d15c5d194b5c9 |  3.5.15 |   20 kB |     false |      false |         2 |          8 |                  8 |        |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
```

```bash
docker exec etcd0 etcdctl endpoint status --endpoints http://etcd1:2380 --write-out="table"
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
|     ENDPOINT      |        ID        | VERSION | DB SIZE | IS LEADER | IS LEARNER | RAFT TERM | RAFT INDEX | RAFT APPLIED INDEX | ERRORS |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
| http://etcd1:2380 | ade526d28b1f92f7 |  3.5.15 |   20 kB |     false |      false |         2 |          8 |                  8 |        |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
```

```bash
docker exec etcd0 etcdctl endpoint status --endpoints http://etcd2:2380 --write-out="table"
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
|     ENDPOINT      |        ID        | VERSION | DB SIZE | IS LEADER | IS LEARNER | RAFT TERM | RAFT INDEX | RAFT APPLIED INDEX | ERRORS |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
| http://etcd2:2380 | d282ac2ce600c1ce |  3.5.15 |   20 kB |      true |      false |         2 |          8 |                  8 |        |
+-------------------+------------------+---------+---------+-----------+------------+-----------+------------+--------------------+--------+
```

Убедиться, что кластер Patroni запущен. Посмотрим по логам:

```bash
docker logs -f pg_patroni0
...
2025-08-28 10:54:32,695 INFO: promoted self to leader by acquiring session lock
server promoting
2025-08-28 10:54:33,709 INFO: Lock owner: pg_patroni0; I am pg_patroni0
2025-08-28 10:54:33,755 INFO: Reaped pid=86, exit status=0
2025-08-28 10:54:34,096 INFO: no action. I am (pg_patroni0), the leader with the lock
```

```bash
docker logs -f pg_patroni1
...
2025-08-28 10:54:32,945 INFO: following new leader after trying and failing to obtain lock
2025-08-28 10:54:33,749 INFO: Lock owner: pg_patroni0; I am pg_patroni1
2025-08-28 10:54:33,788 INFO: Local timeline=1 lsn=0/50000A0
2025-08-28 10:54:33,830 INFO: no action. I am (pg_patroni1), a secondary, and following a leader (pg_patroni0)
2025-08-28 10:54:43,740 INFO: Lock owner: pg_patroni0; I am pg_patroni1
2025-08-28 10:54:43,780 INFO: Local timeline=2 lsn=0/5000180
2025-08-28 10:54:43,810 INFO: primary_timeline=2
2025-08-28 10:54:43,820 INFO: no action. I am (pg_patroni1), a secondary, and following a leader (pg_patroni0)
```

```bash
docker logs -f pg_patroni2
...
2025-08-28 10:54:42,765 INFO: no action. I am (pg_patroni2), a secondary, and following a leader (pg_patroni0)
2025-08-28 10:54:43,740 INFO: Lock owner: pg_patroni0; I am pg_patroni2
2025-08-28 10:54:43,781 INFO: Local timeline=1 lsn=0/40000A0
2025-08-28 10:54:43,809 INFO: primary_timeline=2
2025-08-28 10:54:43,809 INFO: primary: history=1	0/50000A0	no recovery target specified
2025-08-28 10:54:43,844 INFO: no action. I am (pg_patroni2), a secondary, and following a leader (pg_patroni0)
```

Понимаем, что кластер запустился, проверим что лидер выбран:

```bash
docker exec -it pg_patroni1 patronictl -c /etc/patroni/patroni.yml list

+ Cluster: patroni-cluster (7543595759034179610) -+----+-----------+
| Member      | Host        | Role    | State     | TL | Lag in MB |
+-------------+-------------+---------+-----------+----+-----------+
| pg_patroni0 | pg_patroni0 | Leader  | running   |  1 |           |
| pg_patroni1 | pg_patroni1 | Replica | streaming |  1 |         0 |
| pg_patroni2 | pg_patroni2 | Replica | streaming |  1 |         0 |
+-------------+-------------+---------+-----------+----+-----------+
```

Убедимся, что работает прокси-сервер и может пробрасывать соединение до БД. Для этого подключимся к базе (не из хостовой системы, потому что на хосте версия 12, а у нас 16, с этим есть некие проблемы!) из отдельного контейнера с именем **pg**:

```bash
docker run -it --rm --network social_network_net --name pg postgres:16-alpine /bin/sh
psql --host haproxy_lb --port 5000 -U postgres -d postgres
Password for user postgres: 
psql (16.4)
Type "help" for help.

postgres=# select 1;
 ?column? 
----------
        1
(1 row)

postgres=#
```

Также был в HAProxy включен эндпоинт (`/stats`) со статистикой (на порту **8180**). Открыть в браузере и посмотреть статистику для мониторинга HAProxy: `http://localhost:8180/stats`

Распределенная база запущена, но она пуста. Необходимо наполнить тестовыми данными. Для этого с помощью скрипта `generator/generate_posts.py` создадим набор `users.csv`

Скопировать в контейнер **pg** файл `misc/db_users.sql`

```bash
docker cp ./misc/db_users.sql pg:/tmp/db_users.sql
Successfully copied 2.56kB to pg:/tmp/db_users.sql
```

Из контейнера **pg** применить файл к БД, это создаст новую таблицу **users**, а также индекс для поиска (**users_names_btree_idx**)

```bash
psql --host haproxy_lb --port 5000 -U postgres -d postgres -f /tmp/db_users.sql
Password for user postgres: 
CREATE TABLE
CREATE INDEX
/ #
```

Скопировать в контейнер **pg_patroni0** (лидер кластера) файл `generator/users.csv` с набором пользователей

```bash
docker cp generator/users.csv pg_patroni0:/tmp/users.csv
Successfully copied 196MB to pg_patroni0:/tmp/users.csv
```

Применить данные в контейнере **pg_patroni0** (лидер кластера)

```bash
docker exec -it pg_patroni0 psql -U postgres -c "
  COPY users(second_name, first_name, birthdate, biography, city, pwd_hash)
  FROM '/tmp/users.csv' DELIMITER ',' CSV HEADER;"
```

Из контейнера **pg** проверим, сколько данных у нас в таблице на лидере

```bash
psql --host haproxy_lb --port 5000 -U postgres -d postgres -c "SELECT count(*) FROM users;"
Password for user postgres: 
  count  
---------
 1000000
(1 row)
```

Подождем некоторое время, а затем из контейнера **pg** проверим, сколько данных у нас в таблице на реплике (используем порт 5001)

```bash
psql --host haproxy_lb --port 5001 -U postgres -d postgres -c "SELECT count(*) FROM users;"
Password for user postgres: 
  count  
---------
 1000000
(1 row)
```

Убеждаемся что репликация произошла успешно, данные в кластере (мастер и реплики) синхронизированы.

Проверим, что работает балансировка запросов к БД, подключаясь из контейнера **pg** по порту реплики

```bash
psql --host haproxy_lb --port 5001 -U postgres -d postgres -c "SELECT inet_server_addr();"
Password for user postgres: 
 inet_server_addr 
------------------
 172.21.0.16

psql --host haproxy_lb --port 5001 -U postgres -d postgres -c "SELECT inet_server_addr();"
Password for user postgres: 
 inet_server_addr 
------------------
 172.21.0.11
(1 row)

psql --host haproxy_lb --port 5000 -U postgres -d postgres -c "SELECT inet_server_addr();"
Password for user postgres: 
 inet_server_addr 
------------------
 172.21.0.15
(1 row)
```

Для удобства запустим проверку логов с нескольких сервисов

```bash
docker compose -f docker-compose.hw-09.yml logs -f social_srv1 social_srv2 nginx_lb
```

Выполним из хостовой системы несколько запросов на поиск пользователей

```bash
curl -X GET 'http://localhost:6000/user/search?first_name=Ив&last_name=Ив'
```

Убеждаемся, что запросы успешно отработали. А по логам наблюдаем, что Nginx произвел балансировку по Round-Robin и направлял запросы в разные инстансы сервиса **social_network**:

```bash
social_srv1  | 2025-08-29 08:42:50.134552 [DEBUG] (http_user_service.cpp:62) :: handler: GET /user/search
social_srv1  | 2025-08-29 08:42:50.134631 [TRACE] (app_database_service.cpp:156) :: search_user: query to REPLICA tag='haproxy_lb:5001'
social_srv1  | 2025-08-29 08:42:50.147315 [TRACE] (app.cpp:237) :: 172.21.0.20 - - [29/Aug/2025:08:42:50 +0000] "GET /user/search HTTP/1.0" 200 59920 "-" "curl/7.68.0"
nginx_lb     | 172.21.0.1 - - [29/Aug/2025:08:42:50 +0000] "GET /user/search?first_name=\xD0\x98\xD0\xB2&last_name=\xD0\x98\xD0\xB2 HTTP/1.1" 200 59920 "-" "curl/7.68.0"
social_srv2  | 2025-08-29 08:42:51.733652 [DEBUG] (http_user_service.cpp:62) :: handler: GET /user/search
social_srv2  | 2025-08-29 08:42:51.733729 [TRACE] (app_database_service.cpp:156) :: search_user: query to REPLICA tag='haproxy_lb:5001'
social_srv2  | 2025-08-29 08:42:51.746779 [TRACE] (app.cpp:237) :: 172.21.0.20 - - [29/Aug/2025:08:42:51 +0000] "GET /user/search HTTP/1.0" 200 59920 "-" "curl/7.68.0"
nginx_lb     | 172.21.0.1 - - [29/Aug/2025:08:42:51 +0000] "GET /user/search?first_name=\xD0\x98\xD0\xB2&last_name=\xD0\x98\xD0\xB2 HTTP/1.1" 200 59920 "-" "curl/7.68.0"
```

Ручным способом проверку провели, убедились что система работает, балансировка на инстансы сервиса - выполняется, балансировка на инстансы БД - выполняется, репликация БД - выполняется.  
Перейдем к проверке нагрузочным тестом.

### Описание нагрузочного теста

* в качестве нагрузки на чтение используется K6 тест из файла `k6_tests/balanced_search_and_get.js`

* схема нагрузки:
  * в первые 30 секунд - 50 клиент
  * далее в течении 3 минут - 200 клиентов
  * в последние 30 секунд - нагрузка снижается до 0 клиентов

* выполняется нагрузка на запросы чтения по двум эндпойнтам:
  * `/user/search`
  * `/user/get/`

* клиенты для `/user/search` выбирают произвольно один из следующих видов запросов:
  * `/user/search?first_name=Ив&last_name=Ив`
  * `/user/search?first_name=Ал&last_name=Ал`
  * `/user/search?first_name=Сер&last_name=Сер`

* затем для возвращенного списка в цикле пробегаемся по элементам массива и делаем `/user/get/`/

### Тестирование

* разворачиваем систему, запустить тест нагрузки

```bash
docker compose -f docker-compose.hw-09.yml run k6 run --verbose --out experimental-prometheus-rw /tests/balanced_search_and_get.js
```

* даем тесту завершиться, собираем результаты, сворачиваем систему

* разворачиваем систему, запускаем тест нагрузки

```bash
docker compose -f docker-compose.hw-09.yml run k6 run --verbose --out experimental-prometheus-rw /tests/balanced_search_and_get.js
```

* когда основная нагрузка разогреется, например через 1 минуту, выключить один из слейвов PostgreSQL (в текущей конфигурации кворума это может быть **pg_patroni1** или **pg_patroni2**)

```bash
docker compose -f docker-compose.hw-09.yml kill --signal SIGKILL pg_patroni2
[+] Killing 1/1
 ✔ Container pg_patroni2  Killed
```

* убедиться, что инстанс выключен

```bash
docker ps -a
CONTAINER ID   IMAGE                           COMMAND                  CREATED          STATUS                            PORTS           NAMES
85115dbc1777   social_network-pg_patroni2      "/entrypoint.sh /usr…"   2 hours ago      Exited (137) About a minute ago                   pg_patroni2
```

* даем тесту завершиться, собираем результаты, сворачиваем систему

* разворачиваем систему, запускаем тест нагрузки

```bash
docker compose -f docker-compose.hw-09.yml run k6 run --verbose --out experimental-prometheus-rw /tests/balanced_search_and_get.js
```

* когда основная нагрузка разогреется, например через 1 минуту, выключить один из инстансов бекенда

```bash
docker compose -f docker-compose.hw-09.yml kill --signal SIGKILL social_srv2
[+] Killing 1/1
 ✔ Container social_srv2  Killed 
```

* убедиться, что инстанс выключен

```bash
docker ps -a
CONTAINER ID   IMAGE                           COMMAND                  CREATED          STATUS                            PORTS           NAMES
9680c061938f   social_network:9                "./social_network"       2 hours ago      Exited (137) About a minute ago                   social_srv2
```

* дожидаемся окончания теста, собираем результаты

### Анализ результатов тестов

* результаты первого полного прогона

```text
DEBU[0240] Generating the end-of-test summary...        
DEBU[0240] Usage report sent successfully               


  █ TOTAL RESULTS 

    checks_total.......................: 870709 3627.438325/s
    checks_succeeded...................: 99.99% 870707 out of 870709
    checks_failed......................: 0.00%  2 out of 870709

    ✗ search status 200
      ↳  99% — ✓ 6251 / ✗ 2
    ✓ get status 200

    HTTP
    http_req_duration.......................................................: avg=30.6ms min=488.28µs med=31.88ms max=427.94ms p(90)=47.99ms p(95)=51.08ms
      { expected_response:true }............................................: avg=30.6ms min=488.28µs med=31.88ms max=427.94ms p(90)=47.99ms p(95)=51.08ms
    http_req_failed.........................................................: 0.00%  2 out of 870709
    http_reqs...............................................................: 870709 3627.438325/s

    EXECUTION
    iteration_duration......................................................: avg=4.27s  min=1.3ms    med=2.98s   max=12.88s   p(90)=10.28s  p(95)=11.68s 
    iterations..............................................................: 6253   26.050462/s
    vus.....................................................................: 2      min=2           max=200
    vus_max.................................................................: 200    min=200         max=200

    NETWORK
    data_received...........................................................: 572 MB 2.4 MB/s
    data_sent...............................................................: 99 MB  414 kB/s




running (4m00.0s), 000/200 VUs, 6253 complete and 0 interrupted iterations
default ✓ [======================================] 000/200 VUs  4m0s
DEBU[0241] Everything has finished, exiting k6 normally!
```

* результаты второго прогона с выключением экземпляра БД

```text
DEBU[0240] Generating the end-of-test summary...        
DEBU[0240] Usage report sent successfully               


  █ TOTAL RESULTS 

    checks_total.......................: 817834 3405.539928/s
    checks_succeeded...................: 99.99% 817829 out of 817834
    checks_failed......................: 0.00%  5 out of 817834

    ✗ search status 200
      ↳  99% — ✓ 5851 / ✗ 1
    ✗ get status 200
      ↳  99% — ✓ 811978 / ✗ 4

    HTTP
    http_req_duration.......................................................: avg=32.62ms min=466.53µs med=31.15ms max=31.05s p(90)=50.28ms p(95)=57.2ms
      { expected_response:true }............................................: avg=32.59ms min=466.53µs med=31.15ms max=31.05s p(90)=50.28ms p(95)=57.2ms
    http_req_failed.........................................................: 0.00%  5 out of 817834
    http_reqs...............................................................: 817834 3405.539928/s

    EXECUTION
    iteration_duration......................................................: avg=4.57s   min=48.05ms  med=3.27s   max=39.37s p(90)=11.33s  p(95)=12.83s
    iterations..............................................................: 5852   24.368294/s
    vus.....................................................................: 2      min=2           max=200
    vus_max.................................................................: 200    min=200         max=200

    NETWORK
    data_received...........................................................: 538 MB 2.2 MB/s
    data_sent...............................................................: 93 MB  388 kB/s




running (4m00.1s), 000/200 VUs, 5852 complete and 0 interrupted iterations
default ✓ [======================================] 000/200 VUs  4m0s
DEBU[0240] Everything has finished, exiting k6 normally!
```

* результаты третьего прогона с выключением экземпляра сервиса

```text
DEBU[0240] Generating the end-of-test summary...        
DEBU[0240] Usage report sent successfully               


  █ TOTAL RESULTS 

    checks_total.......................: 1071700 4463.65497/s
    checks_succeeded...................: 55.35%  593255 out of 1071700
    checks_failed......................: 44.64%  478445 out of 1071700

    ✗ search status 200
      ↳  2% — ✓ 5520 / ✗ 237446
    ✗ get status 200
      ↳  70% — ✓ 587735 / ✗ 240999

    HTTP
    http_req_duration.......................................................: avg=23.16ms  min=38.17µs  med=9.84ms   max=27.69s p(90)=56.16ms p(95)=62.61ms
      { expected_response:true }............................................: avg=38.81ms  min=493.08µs med=24.19ms  max=27.69s p(90)=61.2ms  p(95)=74.28ms
    http_req_failed.........................................................: 44.64%  478445 out of 1071700
    http_reqs...............................................................: 1071700 4463.65497/s

    EXECUTION
    iteration_duration......................................................: avg=102.59ms min=82.83µs  med=815.47µs max=28.39s p(90)=5.25ms  p(95)=8.85ms 
    iterations..............................................................: 242966  1011.958938/s
    vus.....................................................................: 6       min=2                 max=199
    vus_max.................................................................: 200     min=200               max=200

    NETWORK
    data_received...........................................................: 599 MB  2.5 MB/s
    data_sent...............................................................: 122 MB  507 kB/s




running (4m00.1s), 000/200 VUs, 242966 complete and 0 interrupted iterations
default ✓ [======================================] 000/200 VUs  4m0s
DEBU[0240] Everything has finished, exiting k6 normally!
```

По результатам нагрузочных тестов видим:

* отключение экземпляра слейва PostgreSQL почти никак не повлияло на работоспособность. У нас в системе всё еще остается дополнительная реплика с которой происходит чтение. Latency успешных запросов возросла совсем незначительно (с **p(95)=51.08ms** до **p(95)=57.2ms**). Неудачей завершилось чуть больше запросов, но вероятно это произошло как раз в процессе убийства инстанса БД.

* отключение экземпляра сервиса бекенда повлияло на работоспособность ощутимым образом. У нас в системе всё еще остался дополнительный инстанс сервиса, поэтому latency успешных запросов увеличился незначительно (с **p(95)=51.08ms** до **p(95)=62.61ms**). Однако неудачей стали заканчиваться очень большое количество запросов (**44.64%**), немногим меньше половины, потому что инстанс мы убили почти в самом начале теста.

* в общем и целом, в обоих случаях **система осталась работоспособной**. Несмотря на то, что при отключении половины экземпляров бекенд сервисов **мы видим сильную деградацию, однако система всё ещё отвечает на запросы!**

* проблема с балансировкой бекенд сервисов, я считаю, связана с применением Nginx не коммерческой (open-source) версии:
  * в этой версии у него нет активного health-check
  * он не проверяет серверы в фоне, а отправляет запросы и смотрит на результат, т.е. обнаруживает проблемы уже отправив запрос на мертвый сервер
  * после того как сервер определяется как сбойный, он отключается на время, а затем снова включается и на него снова летят запросы, которые летят вникуда и останутся без ответа

По последнему пункту, видится, что нужно вместо Nginx использовать HAproxy (с активными health-check). Либо искать какие то нетривиальные решения, о которых я пока не знаю.
