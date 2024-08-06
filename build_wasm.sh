#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

AO_IMAGE="aoc:latest" # TODO: Change to remote image when ready
PROCESS_DIR="${SCRIPT_DIR}/process"

cd ${PROCESS_DIR} 
docker run -e DEBUG=1 --platform linux/amd64 -v ./:/src ${AO_IMAGE} ao-build-module

cp ${PROCESS_DIR}/process.wasm ${SCRIPT_DIR}/test-llm/process.wasm
cp ${PROCESS_DIR}/process.js ${SCRIPT_DIR}/test-llm/process.js