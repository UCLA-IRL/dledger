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

For running 2-node test:

```bash

# configure NFD
nfd-start
nfdc strategy set /dledger-multicast /localhost/nfd/strategy/multicast

# forwarder connection setup
nfdc face create udp4://<the other node>:6363
nfdc route add /dledger <face id from last command>
nfdc route add /dledger-multicast <face id from last command>

# generate keys and certificates
for i in a b c d e
do
    ndnsec key-gen /dledger/test-1$i > test-1$i.certreq
    ndnsec key-gen /dledger/test-2$i > test-1$i.certreq
    ndnsec export -P $i /dledger/test-2$i > test-2$i.key
    
    #copy over to the other node
    #on second node
    ndnsec import -P $i - < test-2$i.key
done

ndnsec key-gen /dledger/test-anchor
ndnsec-sign-req /dledger/test-anchor | ndnsec cert-gen -s /dledger/test-anchor - > dledger-anchor.cert 

mkdir test-certs
ndnsec key-gen /dledger/test-a | ndnsec cert-gen -s /dledger/test-anchor - > test-certs/test-a.cert
ndnsec key-gen /dledger/test-b | ndnsec cert-gen -s /dledger/test-anchor - > test-certs/test-b.cert
ndnsec key-gen /dledger/test-c | ndnsec cert-gen -s /dledger/test-anchor - > test-certs/test-c.cert
ndnsec key-gen /dledger/test-d | ndnsec cert-gen -s /dledger/test-anchor - > test-certs/test-d.cert
ndnsec key-gen /dledger/test-e | ndnsec cert-gen -s /dledger/test-anchor - > test-certs/test-e.cert


# run each of the following as a peer
./build/ledger-impl-test test-a
./build/ledger-impl-test test-b
./build/ledger-impl-test test-c
./build/ledger-impl-test test-d
./build/ledger-impl-test test-e
./build/ledger-impl-test-anchor
```
