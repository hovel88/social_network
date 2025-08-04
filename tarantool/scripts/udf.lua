box.cfg{
    listen = '0.0.0.0:3301',
    memtx_memory = 2 * 1024 * 1024 * 1024,  -- 2GB
    checkpoint_interval = 1800,             -- periodic snapshot creation, каждые 30 мин
    wal_mode = 'write',
    log_level = 'verbose'
}

local fiber = require('fiber')
local log   = require('log')
local pg    = require('pg')
local pg_config = {
    host     = 'postgres_db',
    port     = 5432,
    user     = 'postgres',
    password = 'pgpass',
    dbname   = 'postgres'
}

-- пользователь, под которым будет разрешено работать с UDF
box.schema.user.create('tntuser', {password = 'tntpass'})

-- space для кеша пользователей
user_cache = box.schema.space.create('user_cache', {
    if_not_exists = true,
    format = {
        {name = 'id',       type = 'string'},
        {name = 'pwd_hash', type = 'string'}
    }
})
user_cache:create_index('primary', {
    if_not_exists = true,
    type  = 'HASH',
    parts = {'id'}
})

-- хранимая процедура поиска пользователя.
-- если не найдёт такого в кеше, то обратится к PostgreSQL
function check_user(uuid)
    -- log.info(box.session.user())
    log.info('check_user(): uuid=%s', uuid)
    local cached_data = user_cache:get(uuid)
    if cached_data ~= nil then
        log.info({ id = cached_data.id, pwd_hash = cached_data.pwd_hash })
        return {cached_data.id, cached_data.pwd_hash}
    end

    local query_str = [[
SELECT id::text, pwd_hash FROM users WHERE id = '%s' LIMIT 1
]]
    local query = string.format(query_str, uuid)
    log.info('check_user(): query=%s', query)

    local conn = pg.connect(pg_config)
    local res  = conn:execute(query)
    conn:close()

    if res and #res > 0 then
        local row = res[1][1]
        user_cache:replace{row.id, row.pwd_hash}
        log.info({ id = row.id, pwd_hash = row.pwd_hash })
        return {row.id, row.pwd_hash}
    end

    return nil
end

box.schema.func.create('check_user', {if_not_exists=true, setuid=true})
box.schema.user.grant('tntuser', 'execute', 'function', 'check_user')



-- space для кеша диалогов
dialog_cache = box.schema.space.create('dialog_cache', {
    if_not_exists = true,
    format = {
        {name = 'dialog_id',    type = 'string'},   -- UUID сообщения, генерируется внутри PG
        {name = 'created_at',   type = 'unsigned'}, -- timestamp сообщения, генерируется внутри PG
        {name = 'from_user',    type = 'string'},   -- UUID пользователя из users в PG
        {name = 'to_user',      type = 'string'},   -- UUID пользователя из users в PG
        {name = 'message',      type = 'string'},
        {name = 'shard_key',    type = 'string'},   -- ключ для шардирования PG, формат "<from_user>_<to_user>"
        {name = 'is_persisted', type = 'boolean'}   -- флаг сохранения в PG
    }
})
dialog_cache:create_index('primary', {
    if_not_exists = true,
    type  = 'TREE',
    parts = {'dialog_id'}
})
dialog_cache:create_index('dialog_idx', {
    if_not_exists = true,
    type   = 'TREE',
    parts  = {'shard_key', 'created_at'},
    unique = false
})
dialog_cache:create_index('unpersisted', {
    if_not_exists = true,
    type   = 'TREE',
    parts  = {'is_persisted'},
    unique = false
})

-- хранимая процедура отправки диалогового сообщения.
-- в кеш вставляет временную запись, а затем асинхронно сохраняет сообщение в PostgreSQL.
-- если не удалось сохранить в PostgreSQL, удаляем из кеша.
-- если удалось сохранить в PostgreSQL, то обновляем запись в кеше
function send_dialog_message(from_user, to_user, message)
    log.info('send_dialog_message(): from=%s  to=%s', from_user, to_user)
    local temp_dialog_id = require('uuid').str()
    local temp_timestamp = math.floor(os.time() * 1000)
    local shard_key = from_user..'_'..to_user

    -- временная запись
    dialog_cache:insert{temp_dialog_id, temp_timestamp,
                        from_user, to_user, message, shard_key,
                        false} -- is_persisted=false

    -- асинхронно сохраняем в PostgreSQL
    fiber.create(function()
        local query_str = [[
INSERT INTO dialogs (from_user_id, to_user_id, message, shard_key)
VALUES ('%s', '%s', '%s', '%s')
RETURNING dialog_id::text, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms
]]
        local query = string.format(query_str, from_user, to_user, message, shard_key)
        log.info('send_dialog_message(): query=%s', query)

        local conn = pg.connect(pg_config)
        local res  = conn:execute(query)
        conn:close()

        if res and #res > 0 then
            local row = res[1][1]
            log.info('send_dialog_message(): dialog updated (id: %s -> %s, created_at: %d -> %d)', temp_dialog_id, row.dialog_id, temp_timestamp, row.created_at_ms)
            -- Primary Key неизменяем, поэтому dialog_cache:update() или dialog_cache:replace()
            -- с обновлением поля 'dialog_id' приведет к ошибке
            dialog_cache:delete(temp_dialog_id)
            dialog_cache:insert{row.dialog_id, row.created_at_ms,
                                from_user, to_user, message, shard_key,
                                true} -- is_persisted
        else
            log.error('send_dialog_message(): failed to persist message to PostgreSQL')
            -- технически, вместо удаления, можно создать фоновую задачу,
            -- которая будет проверять "is_persisted=false" и повторно
            -- пытаться сохранять их в PostgreSQL через некоторое время
            dialog_cache:delete{temp_dialog_id}
        end
    end)

    return {'ok'}
end

box.schema.func.create('send_dialog_message', {if_not_exists=true, setuid=true})
box.schema.user.grant('tntuser', 'execute', 'function', 'send_dialog_message')


-- хранимая процедура получения списка N последних диалоговых сообщений.
-- изначально берет данные из кеша, и если в нем данных не будет или
-- будет недостаточно, то обратится к PostgreSQL
function list_dialog_messages(from_user, to_user, limit)
    log.info('list_dialog_messages(): from=%s  to=%s  limit=%d', from_user, to_user, limit)
    local shard_key1 = from_user..'_'..to_user
    local shard_key2 = to_user..'_'..from_user
    local cached_min_time = nil
    local unpack_func = table.unpack or unpack
    local cached = {}   -- list of lists
                        -- [ {(string)from_user, (string)to_user, (string)message, (uint)created_at}, {...} ]

    -- собираем из кеша
    for _, shard_key in ipairs({shard_key1, shard_key2}) do
        for _, msg in dialog_cache.index.dialog_idx:pairs({shard_key}, {iterator = 'REQ', limit = limit}) do
            table.insert(cached, {msg.from_user, msg.to_user, msg.message, msg.created_at})

            if not cached_min_time or msg.created_at < cached_min_time then
                cached_min_time = msg.created_at
            end
        end
    end
    log.info('list_dialog_messages(): cached %d element(s)', #cached)
    if #cached >= limit then
        table.sort(cached, function(a, b)
            return a[4] > b[4] -- a.created_at > b.created_at
        end)
        return unpack_func(cached, 1, limit)
    end

    -- догружаем старые сообщения из PostgreSQL
    local query
    if cached_min_time then
        local query_str = [[
SELECT dialog_id::text, from_user_id::text, to_user_id::text, message, shard_key, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms
  FROM dialogs
 WHERE (shard_key = '%s' OR shard_key = '%s')
   AND ((EXTRACT(EPOCH FROM created_at) * 1000)::bigint < %d)
 ORDER BY created_at DESC
 LIMIT %d
]]
        query = string.format(query_str, shard_key1, shard_key2, cached_min_time, limit * 2)
    else
        local query_str = [[
SELECT dialog_id::text, from_user_id::text, to_user_id::text, message, shard_key, (EXTRACT(EPOCH FROM created_at) * 1000)::bigint as created_at_ms
  FROM dialogs
 WHERE (shard_key = '%s' OR shard_key = '%s')
 ORDER BY created_at DESC
 LIMIT %d
]]
        query = string.format(query_str, shard_key1, shard_key2, limit)
    end
    log.info('list_dialog_messages(): query=%s', query)

    local conn = pg.connect(pg_config)
    local res  = conn:execute(query)
    conn:close()

    -- обновляем кеш и добавляем старые сообщения
    local elem_count = 0
    for _, one in ipairs(res) do
        for _, row in ipairs(one) do
            -- log.info('list_dialog_messages(): row.dialog_id=%s  row.created_at_ms=%d', row.dialog_id, tonumber(row.created_at_ms))
            if not dialog_cache:get{row.dialog_id} then
                dialog_cache:replace{row.dialog_id, tonumber(row.created_at_ms),
                                    row.from_user_id, row.to_user_id,
                                    row.message, row.shard_key,
                                    true} -- is_persisted=true
            end

            elem_count = elem_count + 1
            table.insert(cached, {row.from_user_id, row.to_user_id, row.message, tonumber(row.created_at_ms)})
        end
    end
    log.info('list_dialog_messages(): cached %d element(s), inserted %d element(s)', #cached, elem_count)
    table.sort(cached, function(a, b)
        return a[4] > b[4] -- a.created_at > b.created_at
    end)
    return unpack_func(cached, 1, #cached)
end

box.schema.func.create('list_dialog_messages', {if_not_exists=true, setuid=true})
box.schema.user.grant('tntuser', 'execute', 'function', 'list_dialog_messages')



-- tarantool> require('msgpack').decode(('82 22 AA 63 68 65 63 6B 5F 75 73 65 72 21 D9 24 64 62 37 63 31 34 37 63 2D 38 66 66 36 2D 34 64 61 65 2D 61 35 38 66 2D 35 66 36 35 64 30 66 31 63 36 33 36'):gsub(' ', ''):fromhex())
-- ---
-- - {34: 'check_user', 33: 'db7c147c-8ff6-4dae-a58f-5f65d0f1c636'}
-- - 53
-- ...
