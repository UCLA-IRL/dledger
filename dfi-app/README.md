# dfi-app

## Dependency

This branch uses `wasmer` as build dependency. 
The version should be pre-release of v1.0.0

On mac, install by:
```shell script
brew install wasmer
wasmer self-update

# restart your shell
# install pkg-config
wasmer config --pkg-config > $(pkg-config --variable pc_path pkg-config | sed "s/:.*$//g")/wasmer.pc
# you may want to check if CFLAG ends with /include if cmake fails
```
