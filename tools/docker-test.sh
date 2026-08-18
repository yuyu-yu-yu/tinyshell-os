#!/usr/bin/env bash

set -euo pipefail

image_name="tinyshell-os-dev:toolchain-v1"
repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker info >/dev/null
docker build --tag "${image_name}" "${repository_root}"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "${repository_root}:/workspace" \
    --workdir /workspace \
    "${image_name}" \
    make clean test
