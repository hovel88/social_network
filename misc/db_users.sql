--
-- таблица анкет пользователей.
-- первичный ключ: id (тип UUID (SERIAL создает проблемы при репликации))
--
CREATE TABLE IF NOT EXISTS users (
  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  created_at  TIMESTAMP    NOT NULL    DEFAULT NOW(),
  pwd_hash    VARCHAR(100) NOT NULL,
  first_name  VARCHAR(50)  NOT NULL,
  second_name VARCHAR(50)  NOT NULL,
  birthdate   DATE,
  biography   TEXT,
  city        VARCHAR(50)
);

--
-- индекс (btree) для быстрой работы эндпоинта /user/search
--
CREATE INDEX IF NOT EXISTS users_names_btree_idx 
ON users(first_name text_pattern_ops, second_name text_pattern_ops);

--
-- индекс (gin) для быстрой работы эндпоинта /user/search
--
-- CREATE EXTENSION IF NOT EXISTS pg_trgm;
-- CREATE INDEX IF NOT EXISTS users_names_gin_idx 
-- ON users USING GIN(first_name gin_trgm_ops, second_name gin_trgm_ops);
