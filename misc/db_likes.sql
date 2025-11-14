--
-- таблица сведений о лайках (кто поставил, на какой пост, когда, в рамках какой SAGA)
--
CREATE TABLE IF NOT EXISTS post_likes (
    id          BIGSERIAL    PRIMARY KEY,
    user_id     UUID         NOT NULL,
    post_id     UUID         NOT NULL,
    saga_id     VARCHAR(100) NOT NULL,
    op_type     VARCHAR(20)  NOT NULL,
    created_at  TIMESTAMP    DEFAULT NOW()
);

CREATE INDEX idx_post_likes_post_id ON post_likes(post_id);
CREATE INDEX idx_post_likes_user_id ON post_likes(user_id);
CREATE INDEX idx_post_likes_saga_id ON post_likes(saga_id);
