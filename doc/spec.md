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

### Consensus Layer

TBD

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


### P2P Layer

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