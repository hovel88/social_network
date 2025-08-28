#!/bin/sh
# set -eux

if [ "$(id -u)" = "0" ]; then
    # если мы root, настраиваем права каталога PostgreSQL
    # и переключаемся на пользователя postgres
    if [ -d "/var/lib/postgresql/data" ]; then
        chown -R postgres:postgres /var/lib/postgresql/data
        chmod -R 750 /var/lib/postgresql/data

        # очищаем возможные lock файлы
        rm -f /var/lib/postgresql/data/postmaster.pid 2>/dev/null || true
    fi

    exec su-exec postgres "$@"
else
    # если мы уже не root, просто запускаем команду
    exec "$@"
fi
