### Всякое

* узнать список индексов

```bash
docker exec -it postgres_db psql -U postgres -c "\di+"

                                                     List of relations
 Schema |               Name               | Type  |  Owner   | Table | Persistence | Access method |  Size   | Description 
--------+----------------------------------+-------+----------+-------+-------------+---------------+---------+-------------
 public | users_first_name_second_name_idx | index | postgres | users | permanent   | btree         | 9808 kB | 
 public | users_pkey                       | index | postgres | users | permanent   | btree         | 37 MB   | 
(2 rows)
```

* узнать схему конкретного индекса

```bash
docker exec -it postgres_db psql -U postgres -c "\d+ users_first_name_second_name_idx"

                  Index "public.users_first_name_second_name_idx"
   Column    |         Type          | Key? | Definition  | Storage  | Stats target 
-------------+-----------------------+------+-------------+----------+--------------
 first_name  | character varying(50) | yes  | first_name  | extended | 
 second_name | character varying(50) | yes  | second_name | extended | 
btree, for table "public.users"
```

* добавить в образ поддержку команды 'host'

```bash
apk update
apk add bind-tools
# host social_srv
social_srv has address 172.21.0.3
```

* нагрузочный тест

```bash
docker pull skandyla/wrk
docker run -it --rm --network social_network_net --entrypoint=/bin/sh skandyla/wrk

apk update
apk add bind-tools

/data # host social_srv
social_srv has address 172.21.0.3

# тест с настройками сервиса (HTTP_THREADS_COUNT=1, HTTP_QUEUE_CAPACITY=10)
wrk -t 12 -c 400 -d 30s http://172.21.0.3:6000/user/get/8ef1dac2-cf63-473f-82b7-c52876539deb
Running 30s test @ http://172.21.0.3:6000/user/get/8ef1dac2-cf63-473f-82b7-c52876539deb
  12 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     3.63ms   14.32ms   1.67s    99.50%
    Req/Sec   366.86    181.43     1.44k    76.95%
  58382 requests in 30.08s, 27.39MB read
  Socket errors: connect 10, read 0, write 0, timeout 0
Requests/sec:   1940.80
Transfer/sec:      0.91MB

# тест с настройками сервиса (HTTP_THREADS_COUNT=10, HTTP_QUEUE_CAPACITY=100)
wrk -t 12 -c 400 -d 30s http://172.21.0.3:6000/user/get/8ef1dac2-cf63-473f-82b7-c52876539deb
Running 30s test @ http://172.21.0.3:6000/user/get/8ef1dac2-cf63-473f-82b7-c52876539deb
  12 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    24.26ms   59.03ms   1.72s    99.05%
    Req/Sec   343.95    319.58     8.47k    94.27%
  121617 requests in 30.08s, 57.06MB read
  Socket errors: connect 0, read 0, write 0, timeout 23
Requests/sec:   4043.65
Transfer/sec:      1.90MB
```

* мониторинг соединений в PostgreSQL

```bash
docker exec -it postgres_db psql -U postgres -c "SELECT count(*) FROM pg_stat_activity WHERE application_name = 'social_network';"
```
