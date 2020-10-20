#!/bin/bash

if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        # ...
        curl -L https://github.com/bytecodealliance/wasmtime/releases/download/v0.20.0/wasmtime-v0.20.0-x86_64-linux-c-api.tar.xz |\
        tar -xzvf -
        mv wasmtime-v0.20.0-x86_64-linux-c-api wasmtime-c-api

elif [[ "$OSTYPE" == "darwin"* ]]; then
        # Mac OSX
        curl -L https://github.com/bytecodealliance/wasmtime/releases/download/v0.20.0/wasmtime-v0.20.0-x86_64-macos-c-api.tar.xz |\
        tar -xzvf -
        mv wasmtime-v0.20.0-x86_64-macos-c-api wasmtime-c-api
fi

