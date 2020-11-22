# build

To build normal system executable using cmake:
```bash
mkdir build && cd build
cmake ..
make
```

## With wasicc:
To install wasicc:
```shell script
pip install wasienv
``` 

To build wasm using cmake:
```bash
mkdir build && cd build
wasimake cmake ..
emmake make
```

Note that you may need to delete build directory(all temp files) before building
a different target.