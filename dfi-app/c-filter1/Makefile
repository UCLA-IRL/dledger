all: dfi-filter1.wasm

dfi-filter1.wasm: main.c
	wasmcc -Oz -Wl,--allow-undefined ./main.c -o dfi-filter1.wasm
