#!/bin/bash
set -eux

TARGET_IMAGE_NAME=alpine-cpp-builder
TARGET_IMAGE_VERSION=1

docker build \
    -f Dockerfile \
    -t ${TARGET_IMAGE_NAME}:${TARGET_IMAGE_VERSION} .

# -t ${TARGET_IMAGE_NAME}:latest