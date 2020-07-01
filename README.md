# dledger

## Dependencies

* ndn-cxx
* leveldb

* NFD - to forward the NDN network

## Compile

```bash
mkdir build && cd build
cmake ..
make
```

To run the test files

```bash

# configure NFD
nfd-start
nfdc strategy set /dledger-multicast /localhost/nfd/strategy/multicast

# generate keys and certificates
ndnsec key-gen /dledger/test-anchor
ndnsec-sign-req /dledger/test-anchor > dledger-anchor.cert 
ndnsec key-gen /dledger/test-a
ndnsec key-gen /dledger/test-b
ndnsec key-gen /dledger/test-c
ndnsec key-gen /dledger/test-d
ndnsec key-gen /dledger/test-e

# run each of the following as a peer
./build/ledger-impl-test test-a
./build/ledger-impl-test test-b
./build/ledger-impl-test test-c
./build/ledger-impl-test test-d
./build/ledger-impl-test test-e
./build/ledger-impl-test-anchor
```
