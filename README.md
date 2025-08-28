# Сервис социальной сети (курс Highload Architect)

## ДЗ 9: Отказоустойчивость приложений

### Описание

По постановке ДЗ, требуется развернуть отказоустойчивую высокодоступную систему.  

Распространенной реализацией принципа высокой доступности (HA - high availability) в среде PostgreSQL является использование прокси-сервера: вместо прямого подключения к серверу базы данных приложение подключается к прокси-серверу, который перенаправляет запрос к PostgreSQL. А за прокси-сервером располагается кластер из нескольких машин с БД, в котором настроена репликация, чтобы поломка отдельного узла (или нескольких) не привела к потере данных, а также сохранился доступ к PostgreSQL со стороны микросервисов.

В качестве прокси-сервера (и балансировщика нагрузки) хорошо подходит HAproxy. При использовании HAproxy можно также направлять запросы на чтение в одну или несколько реплик для балансировки нагрузки. Однако, приложение должно это учитывать и самостоятельно разделять трафик на чтение и запись. С HAproxy это достигается путем предоставления приложению двух разных портов для подключения. Для этого произведена небольшая доработка в сервисе **social_network**.

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

Для удобства запустим проверку логов с нескольких сервисов

```bash
docker compose -f docker-compose.hw-09.yml logs -f social_srv1 social_srv2 nginx_lb
```

http://localhost:8280/stats




### Описание нагрузочного теста на чтение

* в качестве нагрузки на чтение используется K6 тест из файла `k6_tests/balanced_search_and_get.js`

* схема нагрузки:
  * в первую минуту - 50 клиент  
  * далее в течении 3 минут - 200 клиентов
  * в последнюю минуту - нагрузка снижается до 0 клиентов

* выполняется нагрузка на запросы чтения по двум эндпойнтам:
  * `/user/search`
  * `/user/get/`

* клиенты для `/user/search` выбирают произвольно один из следующих видов запросов:
  * `/user/search?first_name=Ив&last_name=Ив`
  * `/user/search?first_name=Ал&last_name=Ал`
  * `/user/search?first_name=Сер&last_name=Сер`

* затем для возвращенного списка в цикле пробегаемся по элементам массива и делаем `/user/get/`/

### Тестирование

* после разворачивания системы, запустить тест нагрузки

```bash
docker compose -f docker-compose.hw-09.yml run k6 run --verbose --out experimental-prometheus-rw /tests/balanced_search_and_get.js
```


### Выводы
