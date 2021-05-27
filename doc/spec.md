# DLedger current spec, unsolved issues

* Zhiyi Zhang (zhiyi@cs.ucla.edu)

# Current Spec

## Phases

TBD

## Components

Parties:
* peer: an entity with a public/private key pair whose public key can be validated by other peer members and whose private key can be used to sign a record into the DLedger.
* organization: a group of peers belong to an organization. In a simple DLedger system, all peers can belong to the same orginzation.
* identity mananger: each orgnization has a identity manager which will issue public key certificates to entities, making them new peers in the system. They are also responsible for renewing/revoking peer's certificates.

Naming conventions for this spec (Real names can be totally different).
* peers hold the name `/<system-prefix>/<organization-ID>*/<peer-ID>`, e.g., `/example/orgA/alice`.
* identity managers hold the name `/<system-prefix>/<organization-ID>*`, e.g., `/example/orgA`.

**Note**: When there is only one organization in the system, the organization ID is not needed because it can be considered as part of the system prefix.

Global variables:
* `N`, Total number of peers in a DLedger system
* `n`, number of preceding record that each record directly connects to/approve/endorse.
* `num_neighbor`, number of neighbors that each peer connects to

### Record Layer

Record Types: 
* Application data record
* Executable record
* Certificate issuance/renewal record
* Certificate revocation record
* Genesis record

Record format: 
* Name: `/<system-prefix>/<organization-ID>/<peer-ID>/<record-type>/<record-ID>/<timestamp>/<SHA256-digest>`
* Header:
    * A list of record full names as the pointers to preceding records
* Payload
    * Bytes
* Signature
    * Signature info: the name of the signing key. Signature type.
    * Signature value

### Consensus Layer: k out of N consensus

However, in other application scenarios like crypto currency, the consensus requires a confirmation process.
DLedger allows applications to set a (k, N) scheme where n is the total number of peers and k is the number of peers required to confirm a record.
The process can be described in following steps:
* A new record `r_1` produced by `P_i` is added to DAG. At this time, the weight of `r_1` is 0.
* After some time, another peer `P_j` approves the format and content of record `r_1`, and attaches its own new record `r_2` to `r_1`. After this operation, the weight of `r_1` is 1 and the weight of `r_2` is 0.
* Similarly, when new records attach themselves directly or indirectly to `r_1` (e.g., by directly attaching to `r_2`). The weight of `r_1` will increase. Note that the weight is number of peers that have directly or indirectly confirmed the format and content of `r_1`. For example, if `P_j` attach another record to `r_1`, the weight of `r_1` will not increase because `r_1` has already been approved by `P_j`.
* When `r_1`'s weight is equal or larger than k, the record is considered being confirmed by the whole system.


In certain application scenarios where all well-formed records are legitimate, consensus is automatically achieved if the records are linked by any later records. Namely, k = 1 for (k, N).
For example, in a collaborative editing application, if a commit is linked by later records, it means the change is merged. Otherwise, if the record is never linked by later records, the change is discard.

### Ledger Layer

Each peer maintain a DAG.
DAG started with a number of genesis records.
New records are appended to the tail of the DAG by containing `n` pointers to `n` tailing records.
Each poiner contains the hash of the preceding record to make sure the existing records cannot be modified.

Each record is signed by the private key of the peer.

#### Interlock 

It is not allowed to point to the preceding records that are signed by the same peer. This is called the interlock rule.

#### Reputation

Each peer can maintain the reputation of each other peers in the system.
When observing misbehavior, a peer will decrease the reputation score of the corresponding peer.
When some peers' score is lower than a threshold, while still receiving their records into their DAG, the application logic can take certain actions regarding these records, e.g., not attach new records to these records.
These misbavior includes
* Large number of new records in a short time
* When record confirmation is needed, attaching new records to already confirmed records

### Synchronization Layer

All peers form a P2P network with NDN over UDP.
Specifically, each node perform the following operations.

Infrastructure Mode:
* Connects to the NDN overlay network (e.g., NDN testbed) with NDN Find Nearest Neighbor (FCH) services.
* Register the prefix of ``

Infrastructure-less Mode:
* Register themselves into an NDN neighor discovery service (NDS) which can be maintained/developed separately.
* When online, find `num_neighbor` online neighbors by contacting the NDS and create NDN Faces to these neighbors.
* Listen to the sync request prefix `/example/DLR/SYNC` from the network.
* Send triggered (i.e., after generating new records, after receiving outdated DAG view) and periodic (i.e., periodic) sync requests to all neighbors.

# Unsolved Issues