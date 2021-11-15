#! /bin/bash

# PURPOSE:
#
# run clang inside a docker container
#
# USAGE:
#
# Ensure the docker daemon is running.

set -euo pipefail

docker build -t clangenv:dev -f clangenv.Dockerfile .

ENTRYPOINT=$1

shift 1 # pop $1 from $@; we defer tail of $@ to entrypoint

docker run --rm \
  --name clangenv \
  --workdir /work \
  --mount type=bind,source="$(pwd)",target=/work \
  --entrypoint "$ENTRYPOINT" \
  clangenv:dev \
  $@

