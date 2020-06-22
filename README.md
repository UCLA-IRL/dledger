# dledger

## Dependencies

* ndn-cxx
* leveldb

## Compile

```bash
mkdir build && cd build
cmake ..
make
```

To run the test files

```bash
cd build
./backend-test

nfdc strategy set /dledger-multicast /localhost/nfd/strategy/multicast
./ledger-impl-test
```
