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

# generate keys and certificates
ndnsec key-gen /dledger | ndnsec cert-gen -s /dledger - > dledger-anchor.cert 

mkdir test-certs
ndnsec key-gen /dledger/test-a | ndnsec cert-gen -s /dledger - > test-certs/test-a.cert
ndnsec key-gen /dledger/test-b | ndnsec cert-gen -s /dledger - > test-certs/test-b.cert
ndnsec key-gen /dledger/test-c | ndnsec cert-gen -s /dledger - > test-certs/test-c.cert
ndnsec key-gen /dledger/test-d | ndnsec cert-gen -s /dledger - > test-certs/test-d.cert
ndnsec key-gen /dledger/test-e | ndnsec cert-gen -s /dledger - > test-certs/test-e.cert


# run each of the following as a peer
./build/ledger-impl-test test-a
./build/ledger-impl-test test-b
./build/ledger-impl-test test-c
./build/ledger-impl-test test-d
./build/ledger-impl-test test-e
./build/ledger-impl-test-anchor
```
