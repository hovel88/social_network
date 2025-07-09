--
-- таблица соответствия пользователей и их друзей.
-- составной первичный ключ: (user_id, friend_id)
-- ограничение CHECK, для предотвращения дружбы с самим собой
--
CREATE TABLE IF NOT EXISTS friends (
  user_id    UUID      NOT NULL REFERENCES users(id),
  friend_id  UUID      NOT NULL REFERENCES users(id),
  created_at TIMESTAMP NOT NULL DEFAULT NOW(),
  PRIMARY KEY (user_id, friend_id),
  CHECK (user_id <> friend_id)
);

--
-- индексы для быстрого поиска друзей в обоих направлениях
--
CREATE INDEX IF NOT EXISTS friends_user_id_btree_idx 
ON friends(user_id);
CREATE INDEX IF NOT EXISTS friends_friend_id_btree_idx 
ON friends(friend_id);
