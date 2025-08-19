#!/bin/bash
set -eux

BUILDER_IMAGE_NAME=alpine-cpp-builder
BUILDER_IMAGE_VERSION=2
BUILDER_SHELL=/bin/sh
BIND_SERVICE_SRC_DIR=$(pwd)/service
BIND_SERVICE_BUILD_DIR=$(pwd)/service/_build

mkdir -p ${BIND_SERVICE_BUILD_DIR}
rm -rf ${BIND_SERVICE_BUILD_DIR}/*

docker run \
    --rm -it \
    --workdir /app-src \
    -v /etc/group:/etc/group:ro \
    -v /etc/passwd:/etc/passwd:ro \
    -u $(id --user):$(id --group) \
    --mount type=bind,src=${BIND_SERVICE_SRC_DIR},dst=/app-src \
    --mount type=bind,src=${BIND_SERVICE_BUILD_DIR},dst=/install_dir \
    ${BUILDER_IMAGE_NAME}:${BUILDER_IMAGE_VERSION} \
    ${BUILDER_SHELL} make.ws_service.sh

if [ -f ${BIND_SERVICE_BUILD_DIR}/make_image.ws_service.sh ]; then
    cd ${BIND_SERVICE_BUILD_DIR}
    ./make_image.ws_service.sh
fi

# docker run \
#     --rm -it \
#     --workdir /app-src \
#     -v /etc/group:/etc/group:ro \
#     -v /etc/passwd:/etc/passwd:ro \
#     -u $(id --user):$(id --group) \
#     --mount type=bind,src=$(pwd)/service,dst=/app-src \
#     --mount type=bind,src=$(pwd)/service/_build,dst=/install_dir \
#     alpine-cpp-builder:2 \
#     /bin/sh