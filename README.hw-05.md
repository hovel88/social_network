# Сервис социальной сети (курс Highload Architect)

## ДЗ 5: Масштабируемая система диалогов (шардирование)

Для шардирования с учетом эффекта "Леди Гаги", по-моему разумению, может подойти составной ключ шардирования (**from_user_id, to_user_id**).  
Т.е. даже если один пользователь пишет сообщений сильно больше среднего, то такие сообщения потенциально распределятся в разные шарды (чем если бы ключ шардирвоания был простой **from_user_id**).

Также такое разделение по составным ключам позволит достаточно эффективно находить сообщения в диалогах - т.е. все сообщения от **from_user_id** к **to_user_id** будут точно находиться на одном шарде. Хотя сообщения от **to_user_id** к **from_user_id** могут быть на другом шарде, но тоже только на одном.

Но т.к. Citus имеет ограничение и не позволяет использовать несколько колонок для составного ключа шардирования, то используем обходной путь - в таблицу добавим вспомогательное поле **shard_key** типа текст. При вставке новой записи, в него будем генерировать строку, формата **'{from_user_id}_{to_user_id}'**. И это поле будем использовать как ключ шардирования таблицы в Citus.

**ПРИМЕЧАНИЕ:** изначально я попытался указать для поля `GENERATED ALWAYS AS (from_user_id || '_' || to_user_id) STORED` при описании схемы, но такой трюк с Citus также не прошел, пришлось прибегнуть к формированию строки при вставке и сервиса.

Мне кажется, что получился не самый плохой механизм распределения по шардам.

Также, наверное, можно было бы при старте извлекать статистику БД с ID самых активных пользователей, хранить их и при генерации **shard_key** учитывать активность пользователя, чтобы направить диалог в отдельный шард, и таким образом равномерно распределять нагрузку таких активных пользователей по разным шардам.

### Подготовка

* развернуть систему

```bash
docker compose -f docker-compose.service-sharding.yml up -d

# по окончании работы остановить систему командой
docker compose -f docker-compose.service-sharding.yml down --remove-orphans
```

* скопировать в контейнер БД файл `misc/db_users.sql`

```bash
docker cp ./misc/db_users.sql postgres_master:/tmp/db_users.sql
```

* применить файл к БД, это создаст новую таблицу **users**. а также индекс для поиска (**users_names_btree_idx**)

```bash
docker exec -it postgres_master psql -U postgres -f /tmp/db_users.sql
```

* скопировать в контейнер БД файл `misc/db_dialogs.sql`

```bash
docker cp ./misc/db_dialogs.sql postgres_master:/tmp/db_dialogs.sql
```

* применить файл к БД, это создаст новую таблицу **dialogs**. также будут созданы два индекса для быстрого поиска диалогов от одного к другому пользователю и для поля ключа шардирвоания (**dialogs_from_to_btree_idx**, **dialogs_to_from_btree_idx** и **dialogs_shard_key_btree_idx**)

```bash
docker exec -it postgres_master psql -U postgres -f /tmp/db_dialogs.sql
```

* подготовим набор пользователей, как это делалось в ДЗ 2

```bash
docker cp generator/users.csv postgres_master:/tmp/users.csv

docker exec -it postgres_master psql -U postgres -c "
  COPY users(second_name, first_name, birthdate, biography, city, pwd_hash)
  FROM '/tmp/users.csv' DELIMITER ',' CSV HEADER;"
```

* затем получим несколько UUID пользователей, с которыми будем работать далее

```bash
docker exec -it postgres_master psql -U postgres -c "
SELECT id FROM users LIMIT 6;"

                  id                  
--------------------------------------
 22095fc3-1ec7-428c-90ce-5be0e2eebade
 bd498662-1313-4aff-ae1a-2b26e227875b
 385ab26e-8244-4391-8447-9a3bfb21ce21
 360042d9-ce5a-4d07-abc9-50ed02475889
 5bbb0d11-b052-4c43-b3c5-85694d27f13a
 f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5
(6 rows)
```

### Проверка

* подключиться к координатору:

```bash
docker exec -it postgres_master psql -U postgres
```

* создадим референсную таблицу на каждом шарде.  
т.к. в талице **dialogs** у нас есть внешние связи к ID пользователей, не хотелось бы обращаться в запросах к локальной таблице на координаторе по сети, поэтому эта таблица будет реплицирована на каждый узел

```bash
SELECT create_reference_table('users');

NOTICE:  Copying data from local table...
NOTICE:  copying the data has completed
DETAIL:  The local data in the table is no longer visible, but is still on disk.
HINT:  To remove the local data, run: SELECT truncate_local_data_after_distributing_table($$public.users$$)
 create_reference_table 
------------------------
 
(1 row)
```

* создадим распределённую таблицу

```bash
ALTER TABLE dialogs DROP CONSTRAINT dialogs_pkey;
SELECT create_distributed_table('dialogs', 'shard_key');
 create_distributed_table 
--------------------------
 
(1 row)
```

* создадим несколько диалоговых сообщений от пользователя **22095fc3-1ec7-428c-90ce-5be0e2eebade** к **bd498662-1313-4aff-ae1a-2b26e227875b** и обратно:

```bash
curl -X POST http://localhost:6000/dialog/bd498662-1313-4aff-ae1a-2b26e227875b/send \
  -H "Authorization: Bearer 22095fc3-1ec7-428c-90ce-5be0e2eebade" \
  -d '{"text": "привет!"}'
```

```bash
curl -X POST http://localhost:6000/dialog/bd498662-1313-4aff-ae1a-2b26e227875b/send \
  -H "Authorization: Bearer 22095fc3-1ec7-428c-90ce-5be0e2eebade" \
  -d '{"text": "проснись, Neo"}'
```

```bash
curl -X POST http://localhost:6000/dialog/22095fc3-1ec7-428c-90ce-5be0e2eebade/send \
  -H "Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b" \
  -d '{"text": "следуй за розовым слоником)"}'
```

* убедимся, что данные сообщений диалога сохраняются в таблице

```bash
SELECT * FROM dialogs;

              dialog_id               |         created_at         |             from_user_id             |              to_user_id              |                                 shard_key                                 |           message           
--------------------------------------+----------------------------+--------------------------------------+--------------------------------------+---------------------------------------------------------------------------+-----------------------------
 d05a94c9-154c-4258-a534-a07eac77d37d | 2025-07-14 14:05:24.858126 | 22095fc3-1ec7-428c-90ce-5be0e2eebade | bd498662-1313-4aff-ae1a-2b26e227875b | 22095fc3-1ec7-428c-90ce-5be0e2eebade_bd498662-1313-4aff-ae1a-2b26e227875b | привет!
 b076e009-ec53-48e8-a1da-598e0c675919 | 2025-07-14 14:05:37.359756 | 22095fc3-1ec7-428c-90ce-5be0e2eebade | bd498662-1313-4aff-ae1a-2b26e227875b | 22095fc3-1ec7-428c-90ce-5be0e2eebade_bd498662-1313-4aff-ae1a-2b26e227875b | проснись, Neo
 3d707602-9117-4103-8270-e2f6973b932b | 2025-07-14 14:05:44.808643 | bd498662-1313-4aff-ae1a-2b26e227875b | 22095fc3-1ec7-428c-90ce-5be0e2eebade | bd498662-1313-4aff-ae1a-2b26e227875b_22095fc3-1ec7-428c-90ce-5be0e2eebade | следуй за розовым слоником)
(3 rows)
```

* запросим наш небольшой диалог через API

```bash
curl -X GET http://localhost:6000/dialog/22095fc3-1ec7-428c-90ce-5be0e2eebade/list \
  -H "Authorization: Bearer bd498662-1313-4aff-ae1a-2b26e227875b"

[
  {
    "from":"bd498662-1313-4aff-ae1a-2b26e227875b",
    "text":"следуй за розовым слоником)",
    "to":"22095fc3-1ec7-428c-90ce-5be0e2eebade"
  },
  {
    "from":"22095fc3-1ec7-428c-90ce-5be0e2eebade",
    "text":"проснись, Neo",
    "to":"bd498662-1313-4aff-ae1a-2b26e227875b"
  },
  {
    "from":"22095fc3-1ec7-428c-90ce-5be0e2eebade",
    "text":"привет!",
    "to":"bd498662-1313-4aff-ae1a-2b26e227875b"
  }
]
```

* проверим, на каких узлах лежат сейчас данные:

```bash
SELECT nodename, count(*) FROM citus_shards GROUP BY nodename;

             nodename             | count 
----------------------------------+-------
 social_network-postgres_worker-1 |     33
(1 row)
```

* добавим еще несколько шардов:

```bash
docker compose -f docker-compose.service-sharding.yml up --scale postgres_worker=5 -d

[+] Running 8/8
 ✔ Container postgres_master                   Healthy                                      3.3s 
 ✔ Container social_srv                        Running                                      0.0s 
 ✔ Container postgres_citus_manager            Running                                      0.0s 
 ✔ Container social_network-postgres_worker-5  Started                                      7.4s 
 ✔ Container social_network-postgres_worker-2  Started                                      6.1s 
 ✔ Container social_network-postgres_worker-3  Started                                      4.1s 
 ✔ Container social_network-postgres_worker-4  Started                                      4.6s 
 ✔ Container social_network-postgres_worker-1  Running                                      0.5s
```

* убедиться, что координатор видит шарды:

```bash
SELECT master_get_active_worker_nodes();

 master_get_active_worker_nodes 
--------------------------------
 (social_network-postgres_worker-1,5432)
 (social_network-postgres_worker-4,5432)
 (social_network-postgres_worker-5,5432)
 (social_network-postgres_worker-2,5432)
 (social_network-postgres_worker-3,5432)
(5 rows)
```

* перебалансируем данные:

```bash
SELECT rebalance_table_shards('dialogs');
```

* создадим еще несколько диалоговых сообщений, в этот раз от пользователя **5bbb0d11-b052-4c43-b3c5-85694d27f13a** к **f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5** и обратно:

```bash
curl -X POST http://localhost:6000/dialog/f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5/send \
  -H "Authorization: Bearer 5bbb0d11-b052-4c43-b3c5-85694d27f13a" \
  -d '{"text": "А знаешь, как в Париже называют четвертьфунтовый чизбургер?"}'
```

```bash
curl -X POST http://localhost:6000/dialog/5bbb0d11-b052-4c43-b3c5-85694d27f13a/send \
  -H "Authorization: Bearer f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5" \
  -d '{"text": "Что, они не зовут его четвертьфунтовый чизбургер?"}'
```

```bash
curl -X POST http://localhost:6000/dialog/f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5/send \
  -H "Authorization: Bearer 5bbb0d11-b052-4c43-b3c5-85694d27f13a" \
  -d '{"text": "У них там метрическая система. Они вообще там не понимают, что это такое четверть фунта."}'
```

```bash
curl -X POST http://localhost:6000/dialog/5bbb0d11-b052-4c43-b3c5-85694d27f13a/send \
  -H "Authorization: Bearer f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5" \
  -d '{"text": "И как же они его зовут?"}'
```

```bash
curl -X POST http://localhost:6000/dialog/f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5/send \
  -H "Authorization: Bearer 5bbb0d11-b052-4c43-b3c5-85694d27f13a" \
  -d '{"text": "Они зовут его «Королевский чизбургер»."}'
```

* запросим наш диалог через API:

```bash
curl -X GET http://localhost:6000/dialog/5bbb0d11-b052-4c43-b3c5-85694d27f13a/list \
  -H "Authorization: Bearer f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5"

[
  {
    "from":"5bbb0d11-b052-4c43-b3c5-85694d27f13a",
    "text":"Они зовут его «Королевский чизбургер».",
    "to":"f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5"
  },
  {
    "from":"f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5",
    "text":"И как же они его зовут?",
    "to":"5bbb0d11-b052-4c43-b3c5-85694d27f13a"
  },
  {
    "from":"5bbb0d11-b052-4c43-b3c5-85694d27f13a",
    "text":"У них там метрическая система. Они вообще там не понимают, что это такое четверть фунта.",
    "to":"f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5"
  },
  {
    "from":"f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5",
    "text":"Что, они не зовут его четвертьфунтовый чизбургер?",
    "to":"5bbb0d11-b052-4c43-b3c5-85694d27f13a"
  },
  {
    "from":"5bbb0d11-b052-4c43-b3c5-85694d27f13a",
    "text":"А знаешь, как в Париже называют четвертьфунтовый чизбургер?",
    "to":"f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5"
  }
]
```

* проверим, на каких узлах теперь лежат данные.  
видим, что шарды перераспределились по нодам воркеров

```bash
SELECT nodename, count(*) FROM citus_shards GROUP BY nodename;

             nodename             | count 
----------------------------------+-------
 social_network-postgres_worker-1 |     8
 social_network-postgres_worker-2 |     8
 social_network-postgres_worker-3 |     7
 social_network-postgres_worker-4 |     7
 social_network-postgres_worker-5 |     7
(5 rows)
```

* в завершении посмотрим план запроса.  
видим, что запрос ушел на один шард **dialogs_102023**.  
видим, что шард расположен на одном из воркеров (**Node: host=social_network-postgres_worker-5**).  
видим, что при поиске/фильтрации используются созданные индексы **dialogs_to_from_btree_idx_102023**.  
из плана можем заключить, что сначала выполняется фильтрация по **shard_key**, что сокращает количество проверяемых шардов, прежде чем перейти к проверке конкретных **from_user_id** и **to_user_id**

```bash
EXPLAIN SELECT message FROM dialogs WHERE (shard_key = '5bbb0d11-b052-4c43-b3c5-85694d27f13a_f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5' OR shard_key = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5_5bbb0d11-b052-4c43-b3c5-85694d27f13a') AND ((from_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5' AND to_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a') OR (from_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a' AND to_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5')) ORDER BY created_at;

                 QUERY PLAN                                                                                                                                               
--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 Sort  (cost=11041.82..11291.82 rows=100000 width=40)
   Sort Key: remote_scan.worker_column_2
   ->  Custom Scan (Citus Adaptive)  (cost=0.00..0.00 rows=100000 width=40)
         Task Count: 2
         Tasks Shown: One of 2
         ->  Task
               Node: host=social_network-postgres_worker-5 port=5432 dbname=postgres
               ->  Bitmap Heap Scan on dialogs_102023 dialogs  (cost=8.32..12.35 rows=1 width=40)
                     Recheck Cond: (((to_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a'::uuid) AND (from_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5'::uuid)) OR ((to_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5'::uuid) AND (from_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a'::uuid)))
                     Filter: ((shard_key = '5bbb0d11-b052-4c43-b3c5-85694d27f13a_f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5'::text) OR (shard_key = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5_5bbb0d11-b052-4c43-b3c5-85694d27f13a'::text))
                     ->  BitmapOr  (cost=8.32..8.32 rows=1 width=0)
                           ->  Bitmap Index Scan on dialogs_to_from_btree_idx_102023  (cost=0.00..4.16 rows=1 width=0)
                                 Index Cond: ((to_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a'::uuid) AND (from_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5'::uuid))
                           ->  Bitmap Index Scan on dialogs_to_from_btree_idx_102023  (cost=0.00..4.16 rows=1 width=0)
                                 Index Cond: ((to_user_id = 'f6d0ad4b-2b1f-49b0-a24f-47cab694d4e5'::uuid) AND (from_user_id = '5bbb0d11-b052-4c43-b3c5-85694d27f13a'::uuid))
(15 rows)
```
