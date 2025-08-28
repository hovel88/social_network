
FROM postgres:16-alpine

ARG PATRONI_DIR=/etc/patroni
ARG PG_DATA_DIR=/var/lib/postgresql/data

RUN apk update \
 && apk add python3 python3-dev py3-psycopg2 py3-psutil py3-pip curl libpq-dev \
 && pip3 install --no-cache-dir patroni[etcd]==3.2.0 --break-system-packages \
 && pip3 install --no-cache-dir cdiff --break-system-packages \
 && rm -rf /var/cache/apk/*

RUN mkdir -p ${PATRONI_DIR} \
 && chown -R postgres:postgres ${PATRONI_DIR} \
 && chmod -R 755 ${PATRONI_DIR}

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR ${PATRONI_DIR}
ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/bin/patroni", "/etc/patroni/patroni.yml"]
