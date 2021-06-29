# Use Case: Vehicular Network logging system

In a vehicular network, some data like accidents and associated sensor data 
may need to be kept as evidence for later use. For example, in an accident, other cars may be able to
record photos or lidar scans about the accident scene, which will be useful for determining liability later. 

Therefore, we intends to create a system based on NDN vehicular networks between vehicles and the Roadside Units(RSUs)
to keep the data persistent. 

## Model

### Nodes
In this system, we consider an area on the road, where there are
- Roadside Units(RSUs)
- Vehicles that are quickly enter and exit the network.

### Compromise
All of these nodes have certificate for authentication, from RSU manufacturers or vehicle registration.
Both of these kinds of nodes may be compromised, causing Byzantine fault. 
However, we assume that it is relatively hard to have more than 
k vehicles colluded, as it's hard to own and register these cars together. 

### Dynamic Network
As a Vehicular Network, the topology of the system is constantly changing. 
The car nodes are entering and leaving the area, and the wireless connection between cars are intermit. 
Therefore, the ledger needs to tolerate poor network connections and packet losses.

### Low Latency
In this system, the ledger needs to have a low latency to read data, so the information are quickly
synchronized and distributed to other cars entering, in the dynamic network. 
For log writing, the system might have a surge of the data in an event of accident, so it is important
to reduce the overhand of writing and collecting all the records.

### Flooding Prevention
The cars may have incentive to report and flood the system with fake data if they are compromised 
and wants to pretend to have an accident. To prevent that, we need confirmation of the data from other
peers. We require the nodes to confirm other's records when generating new ones, making sure that older records
are being verified. In addition, the nodes can only generate records on top of other's record, which creates an interlock
of records, making sure that the older records are stored and recognized by other peers. 

### Higher-Level Ledger
In the ledger system, it is possible that some area does not have enough cars or RSUs to write the record, 
or insufficient nodes to prevent nodes from manipulating and removing records. Ensure that this does not happen, 
we keep digests of some recent records in the per-area DAG, so that these records and their preceding record cannot be 
removed without being detected. 

### Incentive Question
This ledger's main functionality, recording traffic events, rely on contributions of cars in the area. 
However, we need to provide incentive to the cars for contributing, so they are not simply watching. 
To keep this, we may be able to provide some reward to the cars for generating records in the ledger.
