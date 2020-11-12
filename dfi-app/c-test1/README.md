# build

To build normal system executable using cmake:
```bash
mkdir build && cd build
cmake ..
make
```

To build wasm using cmake:
```bash
mkdir build && cd build
emcmake cmake ..
emmake make
```

To build js+wasm using cmake:
```bash
mkdir build && cd build
emcmake cmake -DJS=1 ..
emmake make
```

Note that you may need to delete build directory(all temp files) before building
a different target.