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
ndnsec key-gen /ndn/dledger/test-anchor
ndnsec-sign-req /ndn/dledger/test-anchor | ndnsec cert-gen -s /ndn/dledger/test-anchor - > dledger-anchor.cert 

mkdir test-certs
ndnsec key-gen /ndn/dledger/test-a | ndnsec cert-gen -s /ndn/dledger/test-anchor - > test-a.cert
ndnsec key-gen /ndn/dledger/test-b | ndnsec cert-gen -s /ndn/dledger/test-anchor - > test-b.cert
ndnsec key-gen /ndn/dledger/test-c | ndnsec cert-gen -s /ndn/dledger/test-anchor - > test-c.cert
ndnsec key-gen /ndn/dledger/test-d | ndnsec cert-gen -s /ndn/dledger/test-anchor - > test-d.cert
ndnsec key-gen /ndn/dledger/CA | ndnsec cert-gen -s /ndn/dledger/test-anchor - > CA.cert


# run each of the following as a peer
./build/ledger-impl-test test-a
./build/ledger-impl-test test-b
./build/ledger-impl-test test-c
./build/ledger-impl-test test-d
./build/ledger-impl-test-anchor
# also the ndncert server
```
