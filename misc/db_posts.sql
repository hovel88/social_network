--
-- таблица постов пользователей.
-- первичный ключ: id (тип UUID (SERIAL создает проблемы при репликации))
--
CREATE TABLE IF NOT EXISTS posts (
    id         UUID      PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMP NOT NULL    DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL    DEFAULT NOW(),
    deleted_at TIMESTAMP             DEFAULT NULL,
    user_id    UUID      NOT NULL    REFERENCES users(id),
    content    TEXT      NOT NULL
);

--
-- индексы для быстрого поиска постов по ID пользователя (/post/feed)
-- и для сортировки
--
CREATE INDEX IF NOT EXISTS posts_user_id_btree_idx 
ON posts(user_id) 
WHERE deleted_at IS NULL;
CREATE INDEX IF NOT EXISTS posts_created_at_btree_idx 
ON posts(created_at DESC) 
WHERE deleted_at IS NULL;

--
-- добавляем новое поле
-- и обновляем все имеющиеся записи
--
ALTER TABLE IF EXISTS posts 
ADD COLUMN IF NOT EXISTS 
    likes_count INTEGER     DEFAULT 0;

UPDATE posts SET likes_count = 0 WHERE likes_count IS NULL;
