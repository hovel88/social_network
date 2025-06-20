# Сервис социальной сети (курс Highload Architect)

## Домашние задания

### ДЗ 1: Ручная проверка

* развернуть сервис и БД (если разворачивается впервые, то не забыть рядом с файлом `docker-compose.yml` создать каталог `postgres_db`)

```bash
docker compose up -d
```

* зарегистрировать пользователя REST-запросом

```bash
curl -X POST http://localhost:6000/user/register \
  -H "Content-Type: application/json" \
  -d '{
    "first_name": "Иван",
    "second_name": "Иванов",
    "birthdate": "1990-01-01",
    "biography": "Программирование, музыка",
    "city": "Москва",
    "password": "secret123"
  }'

{"user_id":"8ef1dac2-cf63-473f-82b7-c52876539deb"}
```

* либо можно засунуть тело в JSON-файл (см. файл `misc/user_register.json`) и выполнить команду

```bash
curl -X POST http://localhost:6000/user/register \
  -H "Content-Type: application/json" \
  -d @user_register.json

{"user_id":"9ddbf14a-7dcb-4b12-88d6-50b0d4fb8990"}
```

* залогиниться REST-запросом

```bash
curl -X POST http://localhost:6000/login \
  -H "Content-Type: application/json" \
  -d '{
    "id": "8ef1dac2-cf63-473f-82b7-c52876539deb",
    "password": "secret123"
  }'

{"token":"8ef1dac2-cf63-473f-82b7-c52876539deb"}
```

* получить анкету пользователя REST-запросом

```bash
curl -X GET http://localhost:6000/user/get/8ef1dac2-cf63-473f-82b7-c52876539deb

{"biography":"Программирование, музыка","birthdate":"1990-01-01","city":"Москва","first_name":"Иван","id":"8ef1dac2-cf63-473f-82b7-c52876539deb","second_name":"Иванов"}
```

* проверить, что таблицы в БД существуют

```bash
docker exec -it postgres_db psql -U postgres -c "\dt"

         List of relations
 Schema | Name  | Type  |  Owner   
--------+-------+-------+----------
 public | users | table | postgres
(1 row)
```

* получить список пользователей запросом к контейнеру БД

```bash
docker exec -it postgres_db psql -U postgres -c "SELECT * FROM users;"

                  id                  |         created_at         |                           pwd_hash                           | first_name | second_name | birthdate  |        biography         |      city       
--------------------------------------+----------------------------+--------------------------------------------------------------+------------+-------------+------------+--------------------------+-----------------
 48dea3d5-ad8a-4d68-bfd5-ebbad025fca9 | 2025-06-19 13:15:16.084019 | $2a$12$AaV3OelRZmET9nuTnYXfruWC7L6oz8BvEPjNPvAY8XiSGjFj/dJqm | Мария      | Петрова     | 1985-05-15 | Путешествия, фотография  | Санкт-Петербург
 8ef1dac2-cf63-473f-82b7-c52876539deb | 2025-06-19 13:15:25.811325 | $2a$12$dq2HNswwr0u1PGQqxdO9muTTRSfMfjoG8I3Yd.hZ/OVCELxGPd29K | Иван       | Иванов      | 1990-01-01 | Программирование, музыка | Москва
(2 rows)
```


## Сборка и настройка сервиса

### Настройка переменными окружения

| Параметр | Допустимый диапазон | По умолчанию | Описание |
|--|--|--|--|
| **HTTP_LISTENING** | `<IP>:[1 .. 65535]` | `"0.0.0.0:6000"` | IP-адрес и порт HTTP сервера, на котором будет запущен listening |
| **HTTP_QUEUE_CAPACITY** | `[1 .. 1024]` | `10` | ёмкость очереди запросов от клиентов HTTP сервера |
| **HTTP_THREADS_COUNT** | `[1 .. 10]` | `1` | количество параллельных потоков для обслуживания очереди запросов от клиентов HTTP сервера |
| | | | |
| **PGSQL_ENDPOINT** | `postgresql://[login[:password]@]<host>:[1 .. 65535]/<database>` | `"postgresql://localhost:5432/postgres"` | URL-эндпойнт для доступа к северу базы данных PostgreSQL |
| **PGSQL_LOGIN** | любые символы кроме `:` | `"postgres"` | логин для авторизации клиента на сервере базы данных PostgreSQL |
| **PGSQL_PASSWORD** |  | `""` | пароль для авторизации клиента на сервере базы данных PostgreSQL |

### Сборка и запуск сервиса

В каталоге `/service` находится исходный код на языке C++ и файл для сборки Docker-образа.

* перейти в каталог `/service`

* выполнить команду  
`docker build -t social_network:latest -f Dockerfile .`

* выполняется multi-stage сборка образа.  
на первой стадии подготавливается образ для компиляции исходных кодов и сама компиляция проекта.  
на второй стадии формируется образ с добавленным в него бинарным файлом

* после успешного выполнения сборки образа сервиса, запустить его можно командой  
`docker run -dt -p 6000:6000 --rm --name social social_network:latest`

### Запуск сервиса вместе с базой данных

В корне проекта находится файл `docker-compose.yml`, который развернет в одной сети контейнер с базой данных PostgreSQL, и контейнер с нашим сервисом.

> **ПРИМЕЧАНИЕ:** в первый раз нужно создать рядом с файлом `docker-compose.yml` каталог `postgres_db`, в котором персистентно будет храниться база данных!

```bash
# запуск и удаление
docker compose up -d
docker compose down
```

### Тестирование

* тест SQL-инъекции при отправке ID (если вернет 400 — защита работает, проверяется формат UUID)

```bash
curl -X POST http://localhost:6000/login \
  -H "Content-Type: application/json" \
  -d '{"id": "1 OR 1=1", "password": "123"}'
```

* нагрузочный тест

```bash
wrk -t12 -c400 -d30s http://localhost:6000/user/get/123
```

* мониторинг соединений в PostgreSQL

```bash
docker exec -it postgres_db psql -U postgres -c "SELECT count(*) FROM pg_stat_activity WHERE application_name = 'social_network';"
```
