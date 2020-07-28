./build/ledger-impl-test-graph test-1a > test-1a.log &
sleep 0.1
./build/ledger-impl-test test-1b > test-1b.log &
sleep 0.1
./build/ledger-impl-test test-1c > test-1c.log &
sleep 0.1
./build/ledger-impl-test test-1d > test-1d.log &
sleep 0.1
./build/ledger-impl-test test-1e > test-1e.log &
sleep 0.1
./build/ledger-impl-test-anchor > test-anchor.log &

echo "started, wait 20s"
sleep 20

nfdc route remove /dledger 266
nfdc route remove /dledger-multicast 266

echo "disconnected, wait 100s"
sleep 100

nfdc route add /dledger 266
nfdc route add /dledger-multicast 266

echo "reconnected, wait 60s"
sleep 60

echo done
