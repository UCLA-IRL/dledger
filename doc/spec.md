# DLedger Protocol Spec

Peer Name:
```
/peer-name
```

Record Name:
```
/peer-name/[record-type]/[record-name]/[time-stamp]
```

Multicast sync:
```
/multicast-prefix/SYNC/[digest]
```

Preceding record Selection:
The current selection algorithm randomize peer names. 
This means each peer has equal chance of being selected regardless of number of its tailing record.
This is useful when some nodes are attacking by flooding new records:
if A floods 100 record, while B only has 1 new record.
B's record will still quickly with each new record use it half the time.