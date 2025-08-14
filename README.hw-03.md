# Сервис социальной сети (курс Highload Architect)

## ДЗ 3: Репликация

### Описание нагрузочного теста на чтение

* в качестве нагрузки на чтение используется K6 тест из файла `k6_tests/search_and_get.js`

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

### Описание нагрузочного теста на запись

* в качестве нагрузки на запись используется K6 тест из файла `k6_tests/user_register.js`

* схема нагрузки:
  * постоянная в течении 4 минут - 100 клиентов

* выполняется нагрузка на запросы записи в эндпоинт `/user/register`
  * имя пользователя формата **"User_${__VU}"**
  * фамилия пользователя формата **"Last_${__ITER}"**
  * остальные данные одинаковые

### Мониторинг результатов

* для сбора и экспорта метрик Docker и некоторых системных показателей в Prometheus дополнительно будет развернуто несколько контейнеров:
  * `prom/node-exporter`
  * `google/cadvisor`

* в файл `monitoring/prometheus/prometheus.yml` с конфигурацией Prometheus, добавлены эти два новых источника метрик

* в Grafana загружены дашборды, в том числе стандартный с ID 893 (`monitoring/grafana/dashboards/cadvisor_prometheus_893.json`) для мониторинга Docker и некоторых системных показателей

* также обновлен дашборд `monitoring/grafana/dashboards/social.json`, в нем появился график, показывающий распределение запросов по хостам БД

### Разворачивание нескольких БД в режиме репликации

* в каталоге `replication` создать 3 подкаталога для данных каждой БД:
  * `postgres_m0_data` - база мастера
  * `postgres_r1_data` - база реплики 1
  * `postgres_r2_data` - база реплики 2

* развернуть контейнер **postgres_m0**

```bash
docker compose -f docker-compose.service-replication.yml run -d postgres_m0
```

* сгенерировать как в ДЗ 1 тестовый набор пользователей

* выполнить создание базы в контейнере **postgres_m0**

```bash
docker exec -it postgres_m0 psql -U postgres -c "
CREATE TABLE IF NOT EXISTS users (
  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at  TIMESTAMP    NOT NULL DEFAULT NOW(),
  pwd_hash    VARCHAR(100) NOT NULL,
  first_name  VARCHAR(50)  NOT NULL,
  second_name VARCHAR(50)  NOT NULL,
  birthdate   DATE,
  biography   TEXT,
  city        VARCHAR(50)
);"
```

* выполнить загрузку тестового набора пользователей в контейнер **postgres_m0**

```bash
docker exec -it postgres_m0 psql -U postgres -c "
COPY users(second_name, first_name, birthdate, biography, city, pwd_hash)
FROM '/tmp/users.csv' DELIMITER ',' CSV HEADER;"
```

* создать индекс в контейнере **postgres_m0**

```bash
docker exec -it postgres_m0 psql -U postgres -c "
CREATE INDEX IF NOT EXISTS users_names_btree_idx
ON users(first_name text_pattern_ops, second_name text_pattern_ops);"
```

* создать роль для репликации в контейнере **postgres_m0**

```bash
docker exec -it postgres_m0 psql -U postgres -c "
CREATE ROLE replicator
WITH LOGIN replication PASSWORD 'rpass';"
```

* при запуске docker compose создалась сеть `social_network_net`, в которой работают контейнеры, запомним подсеть

```bash
docker network inspect social_network_net | grep -ai subnet
                    "Subnet": "172.21.0.0/16",
```

* при запуске docker compose в каталоге `replication/postgres_m0_data` располагаются файлы базы мастера
  * изменить файл `replication/postgres_m0_data/pg_hba.conf`, добавив запись в подсетью:

  ```text
  host    replication     replicator      172.21.0.0/16           md5
  ```

  * изменить файл `replication/postgres_m0_data/postgresql.conf` следующим образом:
    * `ssl = off`
    * `wal_level = replica`
    * `max_wal_senders = 4`

* перезапустить мастер **postgres_m0** для применения настроек

```bash
docker restart postgres_m0
```

* подключиться к контейнеру и сделать бекап базы

```bash
docker exec -it postgres_m0 /bin/sh
mkdir /pgbackup
pg_basebackup -h postgres_m0 -D /pgbackup -U replicator -v -P --wal-method=stream
exit
```

* сохранить бекап базы к себе, а затем распихать по каталогам для реплик

```bash
docker cp postgres_m0:/pgbackup ./replication/pgbackup
cp -r ./replication/pgbackup/* ./replication/postgres_r1_data/
cp -r ./replication/pgbackup/* ./replication/postgres_r2_data/
```

* создать специальный файл, чтобы реплики знали, что они реплики

```bash
touch ./replication/postgres_r1_data/standby.signal
touch ./replication/postgres_r2_data/standby.signal
```

* изменить `./replication/postgres_r1_data/postgresql.conf` для реплики **postgres_r1**

```bash
primary_conninfo = 'host=postgres_m0 port=5432 user=replicator password=rpass application_name=postgres_r1'
```

* изменить `./replication/postgres_r2_data/postgresql.conf` для реплики **postgres_r2**

```bash
primary_conninfo = 'host=postgres_m0 port=5432 user=replicator password=rpass application_name=postgres_r2'
```

* запустить контейнеры реплик **postgres_r1** и **postgres_r2**

```bash
docker compose -f docker-compose.service-replication.yml run -d postgres_r1
docker compose -f docker-compose.service-replication.yml run -d postgres_r2
```

* убедиться, что обе реплики работают в асинхронном режиме на **postgres_m0**

```bash
docker exec -it postgres_m0 psql -U postgres -c "
SELECT application_name, sync_state FROM pg_stat_replication;"

 application_name | sync_state 
------------------+------------
 postgres_r1      | async
 postgres_r2      | async
(2 rows)
```

* включить синхронную репликацию на мастере **postgres_m0**:
  * изменить файл `./replication/postgres_m0_data/postgresql.conf`:
    * `synchronous_commit = on`
    * `synchronous_standby_names = 'FIRST 1 (postgres_r1, postgres_r2)'`
  * перезагрузить конфиг

  ```bash
  docker exec -it postgres_m0 su - postgres -c psql
  SELECT pg_reload_conf();
  exit
  ```

* убедиться, что репликация стала синхронной

```bash
docker exec -it postgres_m0 psql -U postgres -c "
SELECT application_name, sync_state FROM pg_stat_replication;"

 application_name | sync_state 
------------------+------------
 postgres_r1      | sync
 postgres_r2      | potential
(2 rows)
```

### Тест на распределенное чтение

* подготовить систему без учета реплик
  * в файле `docker-compose.service-replication.yml` для сервиса **social_srv** закомментировать переменные окружения:
    * `PGSQL_REPLICA_1_URL`
    * `PGSQL_REPLICA_2_URL`

  * развернуть

  ```bash
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml up -d

  # по окончании работы остановить систему командой
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml down --remove-orphans
  ```

  * запустить тест

  ```bash
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml run k6 run --verbose --out experimental-prometheus-rw /tests/search_and_get.js
  ```

* подготовить систему с учетом реплик
  * в файле `docker-compose.service-replication.yml` для сервиса **social_srv** оставить переменные окружения:
    * `PGSQL_REPLICA_1_URL`
    * `PGSQL_REPLICA_2_URL`

  * развернуть

  ```bash
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml up -d

  # по окончании работы остановить систему командой
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml down --remove-orphans
  ```

  * запустить тест

  ```bash
  docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml run k6 run --verbose --out experimental-prometheus-rw /tests/search_and_get.js
  ```

* результаты тестов K6, чтение только из одной базы

```text
DEBU[0330] Usage report sent successfully               


  █ TOTAL RESULTS 

    checks_total.......................: 682936 2069.492702/s
    checks_succeeded...................: 99.06% 676536 out of 682936
    checks_failed......................: 0.09%  585 out of 682936

    ✗ search status 200
      ↳  99% — ✓ 7166 / ✗ 10
    ✗ get status 200
      ↳  99% — ✓ 668847 / ✗ 4884

    HTTP
    http_req_duration.......................................................: avg=39.99ms min=0s       med=2.9ms  max=17.07s p(90)=86.18ms p(95)=129.35ms
      { expected_response:true }............................................: avg=40.37ms min=287.22µs med=2.97ms max=17.07s p(90)=86.78ms p(95)=130.05ms
    http_req_failed.........................................................: 0.93%  6400 out of 682936
    http_reqs...............................................................: 682936 2069.492702/s

    EXECUTION
    iteration_duration......................................................: avg=3.04s   min=12.01ms  med=1.96s  max=1m26s  p(90)=6.85s   p(95)=8.9s    
    iterations..............................................................: 9067   27.47562/s
    vus.....................................................................: 1      min=1              max=200
    vus_max.................................................................: 200    min=200            max=200

    NETWORK
    data_received...........................................................: 525 MB 1.6 MB/s
    data_sent...............................................................: 79 MB  238 kB/s




running (5m30.0s), 000/200 VUs, 9067 complete and 138 interrupted iterations
default ✓ [======================================] 000/200 VUs  5m0s
```

* результаты тестов K6, распределенное чтение из реплик

```text
DEBU[0330] Usage report sent successfully               


  █ TOTAL RESULTS 

    checks_total.......................: 640916 1945.695915/s
    checks_succeeded...................: 99.93% 640478 out of 640916
    checks_failed......................: 0.06%  438 out of 640916

    ✗ search status 200
      ↳  99% — ✓ 7196 / ✗ 9
    ✗ get status 200
      ↳  99% — ✓ 633282 / ✗ 429

    HTTP
    http_req_duration.......................................................: avg=46.11ms min=0s       med=3.59ms max=18.24s p(90)=115.03ms p(95)=144.31ms
      { expected_response:true }............................................: avg=46.14ms min=296.27µs med=3.59ms max=18.24s p(90)=115.06ms p(95)=144.33ms
    http_req_failed.........................................................: 0.06%  438 out of 640916
    http_reqs...............................................................: 640916 1945.695915/s

    EXECUTION
    iteration_duration......................................................: avg=3.95s   min=41.23ms  med=3.1s   max=1m23s  p(90)=7.56s    p(95)=8.59s   
    iterations..............................................................: 7072   21.469212/s
    vus.....................................................................: 2      min=1             max=200
    vus_max.................................................................: 200    min=200           max=200

    NETWORK
    data_received...........................................................: 496 MB 1.5 MB/s
    data_sent...............................................................: 74 MB  226 kB/s




running (5m29.4s), 000/200 VUs, 7072 complete and 133 interrupted iterations
default ✓ [======================================] 000/200 VUs  5m0s
```

* результаты тестов по дашборду Grafana, чтение только из одной базы
![чтение, без реплик](misc/hw3_read_no-replicas.png)

* результаты тестов по дашборду Grafana, распределенное чтение из реплик
![чтение, 2 реплики](misc/hw3_read_replicas.png)

**ВЫВОДЫ:**

1. удалось распределить чтение по репликам, полностью сняв нагрузку на чтение с ноды мастера
2. в случае чтения без репликации, все запросы уходят на единственный узел **postgres_m0**
3. в случае чтения с репликацией, т.к. выполняются запросы только на чтение, на графиках не видно никаких обращений к хосту **postgres_m0**, все запросы поступают на реплики и распределяются поровну
4. распределение выбора между репликами осуществляется по алгоритму Round Robin, этим объясняется одинаковость графиков для хостов **postgres_r1** и **postgres_r2**, т.к. запросы распределяются по ним равномерно
5. результаты тестов K6 показывают несколько ошибок, а на графиках их нет, это связано с тем, что некоторое количество запросов "/user/search" выполняется долго и K6 отваливается по таймауту, списывая их в ошибку, но по метрикам в Prometheus видно, что запрос по итогу отрабатывает успешно

### Тест на запись в реплицированную базу

* снова развернуть

```bash
docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml up -d
# по окончании работы остановить систему командой
docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml down --remove-orphans
```

* запустить тест

```bash
docker compose -f docker-compose.service-replication.yml -f docker-compose.monitoring.yml -f docker-compose.loadtest.yml run k6 run --verbose --out experimental-prometheus-rw /tests/user_register.js
```

* где-то через 2 минуты остановить реплику **postgres_r1**

```bash
docker compose -f docker-compose.service-replication.yml stop postgres_r1
```

* дождаться окончания нагрузки на запись
  * результаты по тесту K6

  ```text
  DEBU[0270] Generating the end-of-test summary...        


  █ TOTAL RESULTS 

    HTTP
    http_req_duration.......................................................: avg=4.76s min=189.85ms med=756.27ms max=29.26s p(90)=9.51s p(95)=9.56s
      { expected_response:true }............................................: avg=4.76s min=189.85ms med=756.27ms max=29.26s p(90)=9.51s p(95)=9.56s
    http_req_failed.........................................................: 0.00%  0 out of 4970
    http_reqs...............................................................: 4970   18.407051/s

    EXECUTION
    iteration_duration......................................................: avg=4.77s min=189.96ms med=768.83ms max=29.26s p(90)=9.51s p(95)=9.56s
    iterations..............................................................: 4970   18.407051/s
    vus.....................................................................: 87     min=87        max=100
    vus_max.................................................................: 100    min=100       max=100

    NETWORK
    data_received...........................................................: 1.7 MB 6.2 kB/s
    data_sent...............................................................: 1.8 MB 6.5 kB/s




  running (4m30.0s), 000/100 VUs, 4970 complete and 87 interrupted iterations
  default ✓ [======================================] 100 VUs  4m0s
  ```

  * результаты по графикам в дашборде в Grafana
  ![запись](misc/hw3_write.png)

* проверить на мастере **postgres_m0** и на оставшейся реплике ****postgres_r2****, сколько данных мы записали.  
убедиться, что количество совпадает (5057 == 5057), транзакции не потерялись

```bash
docker exec -it postgres_m0 psql -U postgres -c "
SELECT count(*) FROM users
WHERE first_name LIKE 'User_%' AND second_name LIKE 'Last_%';"

 count 
-------
  5057
(1 row)

```bash
docker exec -it postgres_r2 psql -U postgres -c "
SELECT count(*) FROM users
WHERE first_name LIKE 'User_%' AND second_name LIKE 'Last_%';"

 count 
-------
  5057
(1 row)
```

* остановить мастер **postgres_m0**

```bash
docker compose -f docker-compose.service-replication.yml stop postgres_m0
```

* снова запустить реплику **postgres_r1**

```bash
docker compose -f docker-compose.service-replication.yml start postgres_r1
```

* проверить на реплике **postgres_r1**, сколько данных мы записали.  
убедиться, что количество отличается (2706 < 5057)

```bash
docker exec -it postgres_r1 psql -U postgres -c "
SELECT count(*) FROM users
WHERE first_name LIKE 'User_%' AND second_name LIKE 'Last_%';"

 count 
-------
  2706
(1 row)
```

* самый свежий слейв это реплика **postgres_r2**

* перенастроить реплику **postgres_r2**, включаем синхронную кворумную репликацию.  
  * отредактировать файл `replication/postgres_r2_data/postgresql.conf`:
    * `synchronous_commit = on`
    * `synchronous_standby_names = 'ANY 1 (postgres_m0, postgres_r1)'`

  * перезагрузить конфиг

  ```bash
  docker exec -it postgres_r2 su - postgres -c psql
  SELECT pg_reload_conf();
  exit
  ```

* переключить отстающую реплику **postgres_r1** на новый мастер
  * отредактировать файл `replication/postgres_r1_data/postgresql.conf`:
    * `primary_conninfo = 'host=postgres_r2 port=5432 user=replicator password=rpass application_name=postgres_r1'`

  * перезагрузить конфиг

  ```bash
  docker exec -it postgres_r1 su - postgres -c psql
  SELECT pg_reload_conf();
  exit
  ```

* промоутим реплику до мастера

```bash
docker exec -it postgres_r2 su postgres -c 'pg_ctl promote -D $PGDATA'
waiting for server to promote.... done
server promoted
```

* убедиться что **postgres_r1** подключился как реплика к **postgres_r2**

```bash
docker exec -it postgres_r2 psql -U postgres -c "
SELECT application_name, sync_state FROM pg_stat_replication;"
```

* реплике **postgres_r1** потребуется немного времени для синхронизации, по окончании проверим наличие данных.  
убедиться, что количество совпадает (транзакции не потеряны)

```bash
docker exec -it postgres_r1 psql -U postgres -c "
SELECT count(*) FROM users
WHERE first_name LIKE 'User_%' AND second_name LIKE 'Last_%';"
```
