#!/usr/bin/env python3

import psycopg2
import uuid
from faker import Faker
from datetime import datetime, timedelta
import random

FILE_POSTS_RAW = "posts.txt"

def read_lines_from_file(filename):
    """
    Аргументы:
        filename: Имя файла, который нужно прочитать.

    Возвращает:
        Список строк из файла.
    """
    try:
        with open(filename, 'r', encoding='utf-8') as file:
            lines = file.readlines()
        return [item.strip() for item in lines] # подчищаем от лишних символов
    except FileNotFoundError:
        print(f"Ошибка: Файл '{filename}' не найден.")
        return None
    except Exception as e:
        print(f"Ошибка при чтении файла: {e}")
        return None

posts = read_lines_from_file(FILE_POSTS_RAW)
fake  = Faker('ru_RU')

def generate_posts(conn_str, posts_per_user=300):
    conn = psycopg2.connect(conn_str)
    cur = conn.cursor()

    # user_id (UUID) генерируется на стороне БД, поэтому надо получить
    # ID всех пользователей уже после их создания
    cur.execute("SELECT id FROM users")
    users = [row[0] for row in cur.fetchall()]

    if not users:
        raise ValueError("В базе нет пользователей")

    users_max_cnt = len(users)-1
    posts_max_cnt = len(posts)-1

    # используем только 100 произвольных пользователей
    user_ids = []
    for _ in range(100):
        user_ids.append(users[fake.random_int(min=0, max=users_max_cnt)])

    # генерация постов
    for user_id in user_ids:
        for _ in range(posts_per_user):
            post_date = datetime.now() - timedelta(days=random.randint(0, 365))

            cur.execute(
                """INSERT INTO posts (user_id, content, created_at, updated_at)
                   VALUES (%s, %s, %s, %s)""",
                # (user_id, fake.text(max_nb_chars=500), post_date, post_date)
                (user_id, posts[fake.random_int(min=0, max=posts_max_cnt)], post_date, post_date)
            )

    conn.commit()
    cur.close()
    conn.close()

if __name__ == "__main__":
    generate_posts("host=localhost port=15432 dbname=postgres user=postgres password=pgpass")
