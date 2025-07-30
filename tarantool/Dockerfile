FROM tarantool/tarantool:3.3.2

RUN apt-get update \
 && apt-get install -y libpq-dev git cmake luarocks \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

RUN luarocks install https://raw.githubusercontent.com/tarantool/pg/master/pg-scm-1.rockspec --global
