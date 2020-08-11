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

# generate anchor certificates
ndnsec key-gen /dledger/test-anchor
ndnsec-sign-req /dledger/test-anchor | ndnsec cert-gen -s /dledger/test-anchor - > dledger-anchor.cert 

# copy the anchor certificate to the other node

# generate keys and certificates
for i in a b c d e
do
    ndnsec key-gen /dledger/test-1$i | ndnsec cert-gen -s /dledger/test-anchor - > test-1$i.cert
    ndnsec key-gen /dledger/test-2$i > /dev/null
    ndnsec export -P $i /dledger/test-2$i > test-2$i.key
    
    #copy over to the other test-2* keys and test-1* certificates
    #on second node
    ndnsec import -P $i - < test-2$i.key
done

# run each of the following on the machine to test
# on node 1
./test/test-scripts/node1-exec.sh

# on node 2
./test/test-scripts/node2-exec.sh
```
