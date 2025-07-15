--
-- таблица со всеми сообщениями диалогов между двумя пользователями
-- (from_user_id, to_user_id)
--
CREATE TABLE IF NOT EXISTS dialogs (
    dialog_id    UUID      PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at   TIMESTAMP NOT NULL DEFAULT NOW(),
    from_user_id UUID      NOT NULL REFERENCES users(id),
    to_user_id   UUID      NOT NULL REFERENCES users(id),
    shard_key    TEXT      NOT NULL,
    message      TEXT      NOT NULL
);

--
-- индексы для быстрого поиска сообщений в диалогах в обоих
-- направлениях, а также для поля ключа шардирования
--
CREATE INDEX IF NOT EXISTS dialogs_from_to_btree_idx 
ON dialogs(from_user_id, to_user_id);
CREATE INDEX IF NOT EXISTS dialogs_to_from_btree_idx 
ON dialogs(to_user_id, from_user_id);
CREATE INDEX IF NOT EXISTS dialogs_shard_key_btree_idx 
ON dialogs(shard_key);
