#!/bin/sh

source .env

docker build -t ki-rom-builder .
docker run --rm -v $(pwd):/opt/src \
    -v ${MAME_ROMS_DIR}/kinst:/mnt/kinst \
    -v ${MAME_ROMS_DIR}/kinst2:/mnt/kinst2 \
    --entrypoint /bin/bash \
    -it ki-rom-builder
