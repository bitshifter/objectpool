#!/bin/sh

# This script's directory (build/) and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DIR=linux-gcc
GEN="Unix Makefiles"

mkdir -p "${SCRIPT_DIR}/${DIR}/debug"
mkdir -p "${SCRIPT_DIR}/${DIR}/release"

cd "${SCRIPT_DIR}/${DIR}/debug"
cmake -G "${GEN}" -DCMAKE_BUILD_TYPE=Debug "${PROJECT_DIR}"
cd -

cd "${SCRIPT_DIR}/${DIR}/release"
cmake -G "${GEN}" -DCMAKE_BUILD_TYPE=Release "${PROJECT_DIR}"
cd -
