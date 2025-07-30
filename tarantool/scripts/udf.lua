box.cfg{
    listen = '0.0.0.0:3301',
    log_level = 'verbose'
}

-- space для кеша пользователей
user_cache = box.schema.space.create('user_cache', {
    format = {
        {name = 'id',       type = 'string'},
        {name = 'pwd_hash', type = 'string'}
    },
    if_not_exists = true})
user_cache:create_index('primary', {
    type  = 'hash',
    parts = {'id'},
    if_not_exists = true
})

local pg  = require('pg')
local log = require('log')

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

    local conn = pg.connect({
        host     = 'postgres_db',
        port     = 5432,
        user     = 'postgres',
        password = 'pgpass',
        dbname   = 'postgres'
    })

    local query = string.format(
        "SELECT id::text, pwd_hash FROM users WHERE id = '%s' LIMIT 1",
        uuid)
    log.info('check_user(): query=%s', query)

    local res = conn:execute(query)
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
box.schema.user.create('tntuser', {password = 'tntpass'})
box.schema.user.grant('tntuser', 'execute', 'function', 'check_user')

-- tarantool> require('msgpack').decode(('82 22 AA 63 68 65 63 6B 5F 75 73 65 72 21 D9 24 64 62 37 63 31 34 37 63 2D 38 66 66 36 2D 34 64 61 65 2D 61 35 38 66 2D 35 66 36 35 64 30 66 31 63 36 33 36'):gsub(' ', ''):fromhex())
-- ---
-- - {34: 'check_user', 33: 'db7c147c-8ff6-4dae-a58f-5f65d0f1c636'}
-- - 53
-- ...
