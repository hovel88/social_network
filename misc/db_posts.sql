--
-- таблица постов пользователей.
-- первичный ключ: id (тип UUID (SERIAL создает проблемы при репликации))
--
CREATE TABLE posts (
    id         UUID      PRIMARY KEY DEFAULT gen_random_uuid(),
    created_at TIMESTAMP NOT NULL    DEFAULT NOW(),
    updated_at TIMESTAMP NOT NULL    DEFAULT NOW(),
    deleted_at TIMESTAMP             DEFAULT NULL,
    user_id    UUID      NOT NULL    REFERENCES users(id),
    content    TEXT      NOT NULL
);

--
-- индексы для быстрого поиска постов по ID пользователя (/post/feed)
--
CREATE INDEX posts_user_id_btree_idx 
ON posts(user_id) WHERE deleted_at IS NULL;
