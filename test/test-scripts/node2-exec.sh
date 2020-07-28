./build/ledger-impl-test test-2a > test-2a.log &
sleep 0.1
./build/ledger-impl-test test-2b > test-2b.log &
sleep 0.1
./build/ledger-impl-test test-2c > test-2c.log &
sleep 0.1
./build/ledger-impl-test test-2d > test-2d.log &
sleep 0.1
./build/ledger-impl-test test-2e > test-2e.log &

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
