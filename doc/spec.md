# DLedger current spec, unsolved issues

* Zhiyi Zhang (zhiyi@cs.ucla.edu)

# 1. Current Spec

## 2. Termologies

Parties:
* peer: an entity with a public/private key pair whose public key can be validated by other peer members and whose private key can be used to sign a record into the DLedger.

Naming conventions for this spec (Real names can be totally different).
* peers hold the name `/<system-prefix>/<peer-ID>`, e.g., `/example/dledger/orgA/alice`.

Global variables:
* `N`, Total number of peers in a DLedger system
* `n`, number of preceding record that each record directly connects to/approve/endorse.
* `num_neighbor`, number of neighbors that each peer connects to.
* `T_max_confirm`, the maximum time for a record to be confirmed. If the record cannot be confirmed after this time, it is considered as an invalid record.

## 3. Components

### 3.1 Synchronization Layer

All peers form a P2P network with NDN (over UDP/TCP/IP).
Specifically, each node perform the following operations.

Infrastructure Mode:
(assumed routing protocol or self-learning protocol is running in the infrastructure)
* Connects to the NDN overlay network (e.g., NDN testbed) with NDN Find Nearest Neighbor (FCH) services.
* Register the prefix `/<system-prefix>/<peer-ID>` and prefix `/<system-prefix>/SYNC` to the network.
* Set the `/<system-prefix>/SYNC` to be a multicast prefix.
* Listen to the sync request multicast prefix `/<system-prefix>/SYNC`.

Infrastructure-less Mode:
* Register themselves into an NDN neighor discovery service (NDS) which can be maintained/developed separately.
* When online, find `num_neighbor` online neighbors by contacting the NDS and create NDN Faces to these neighbors.
* Register the prefix `/<system-prefix>` to all neighbors.
* Set the `/<system-prefix>` to be a multicast prefix.
* Listen to the sync request multicast prefix `/<system-prefix>`.

After that, each peer will
* Send triggered (i.e., after generating new records, after receiving outdated DAG view) and periodic (i.e., periodic) sync requests to the P2P network.
* Answer Interest packets asking for the record Data packets. The record data are named under `/<system-prefix>/<organization-ID>/<peer-ID>`.

### 3.2 Ledger Layer

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
* Attaching new records to already confirmed records
* Appending records that turned out to be invalid, i.e., not eventually confirmed by the system

### 3.3 Authentication Layer:
Each record that is authenticated according to a trust schema. 
Each peer is loaded with the specific trust model, policies and anchors for the ledger during bootstrapping in order to authenticate record. 
In order to maintain changes to the certificate storage, the new information and be added or removed with certificate and revocation records.
These changes will be effective after the record is confirmed. 

DLedger supports a modular design for the trust anchor. The trust manager can be replaced according to the model. 

For Convenience, we discuss an example of the trust model with multiple organization:

There are multiple organization maintaining the ledger collectively. Each organization has its own trust anchor, 
and the corresponding identity management peer. 
Some Terminology:
* peer: A recording generating peer as above. To be a legitimate peer, it belongs to an organization and is authenticated by it. 
* organization: a group of peers belong to an organization. In a simple DLedger system, all peers can belong to the same orginzation.
* identity mananger: each orgnization has a identity manager which will issue public key certificates to entities, making them new peers in the system. They are also responsible for renewing/revoking peer's certificates.

Naming conventions for this spec (Real names can be totally different).
* peers hold the name `/<system-prefix>/<peer-ID>`, e.g., `/example/orgA/alice`.
* identity managers hold the name `/<system-prefix>/<organization-ID>*`, e.g., `/example/orgA`.
**Note**: When there is only one organization in the system, the organization ID is not needed because it can be considered as part of the system prefix.

### 3.4 Consensus Layer: k out of N consensus

However, in other application scenarios like crypto currency, the consensus requires a confirmation process.
DLedger allows applications to set a (k, N) scheme where n is the total number of peers and k is the number of peers required to confirm a record.
The process can be described in following steps:
* A new record `r_1` produced by `P_i` is added to DAG. At this time, the weight of `r_1` is 0.
* After some time, another peer `P_j` approves the format and content of record `r_1`, and attaches its own new record `r_2` to `r_1`. After this operation, the weight of `r_1` is 1 and the weight of `r_2` is 0.
* Similarly, when new records attach themselves directly or indirectly to `r_1` (e.g., by directly attaching to `r_2`). The weight of `r_1` will increase. Note that the weight is number of peers that have directly or indirectly confirmed the format and content of `r_1`. For example, if `P_j` attach another record to `r_1`, the weight of `r_1` will not increase because `r_1` has already been approved by `P_j`.
* When `r_1`'s weight is equal or larger than k, the record is considered being confirmed by the whole system.

In certain application scenarios where all well-formed records are legitimate, consensus is automatically achieved if the records are linked by any later records. Namely, k = 1 for (k, N).
For example, in a collaborative editing application, if a commit is linked by later records, it means the change is merged. Otherwise, if the record is never linked by later records, the change is discard.

### 3.4 Record Layer

Record Types: 
* Application data record (carrying application data like logs)
* Executable record (carrying compiled WASI bytecode that can be executed)
* Certificate issuance/renewal record (carrying certificate issuance and renewal information. Signed by an identity manager.)
* Certificate revocation record (carrying certificate revocation information. Signed by an identity manager.)
* Genesis record (hard-coded records)

Record format: 
* Name: `/<system-prefix>/<organization-ID>/<peer-ID>/<record-type>/<record-ID>/<timestamp>/<SHA256-digest>`
* Header:
    * A list of record full names as the pointers to preceding records
* Payload
    * Bytes
* Signature
    * Signature info: the name of the signing key. Signature type.
    * Signature value

## 4. Phases

### 4.1 Bootstrapping phase

An entity takes the following steps to join the system:
* Generate a ECDSA key pair and obtain a public key certificate from an identity manager.
* Join the P2P network as detailed in Section 3.1.
* After the identity manager appends a certificate issuance record into the ledger and the record gets confirmed, the entity is considered to finish the bootstrap and is able to add new records into the system.

### 4.2 Validation phase

When the entity wants to add a new record, it takes the following steps
* Randomly find `n` records that are not generated by its own from the tailing records of the DAG.
* Verify the format (including digital signature) and the content of each record and their unconfirmed ancesters. Depth First Search can be used in this step.
* If the validation of any record fails, mark this record and all its child records as incorrect records and go back to the first step to find another tailing record.
* Once there are `n` valid tailing records and their ancestors are also valid, append the new record to these `n`records by putting the names of these `n` records into the record's header.
* Finalize the record by signing it.

### 4.3 Synchronization phase

After generating the new record, the entity takes the following step to make the record availble to the network.
* Generate a new sync Interest packet carrying your latest tailing record list, including the newly added record.
* Send this sync Interest packet to the network.
* The sync Interest will be forwarded with pandemic forwarding strategy. But duplicate request will be suppressed with NDN's stateful forwarding plane.
* After sending out the sync Interest packet, other peers will send Interest packet to fetch this newly added record. Multiple requests can be merged or satisfied by cache with NDN's stateful forwarding plane.
* If there is no incoming Interest asking for the new record, the entity should resend the sync Interest.

### 4.4 Confirmation phase

After some time, the peer will receive new records generated by other peers. Once its record is approved directly or indirectly by more than k peers, its record is considered to be confirmed.
If the record cannot be confirmed after a pre-defined time `T_max_confirm`, the record is considered invalid.

# Validation Policies

In DLedger, there are 2 sets of policies: 

## Validation Policies
This policy is enforced when the peers fetched the record content. If the records failed this policy, 
it will be dropped by the receiving peer. 

The specific policies are:
- Format Validity. The record has to have the correct record format. The certificate/revocation record format. 
- Signature Validity. The record has to be signed by the certificates in a confirmed record. 
- Interlock Policy. The record cannot reference the sending peer's own record. 

## Endorsement Policies
This policy is checked when the peers tries to endorse the record by sending a record referring to it. 
It the record fail this policy, the verify peer should choose other record to refer to.

The specific policies are:
- Revocation Check. The record's certificate is not revoked by a confirmed revocation record yet.
- Contribution policies. Each preceding record of The record being verified must have a weight lower than a threshold. 
- Preceding record Validity. The preceding records must be also valid for endorsement. 

### Custom policies
The application can add custom policies to both Validation and Endorsement Policy. 


# Current status and unsolved issues

## 1. Use cases

Applications where all records are supposed to be added and confirmed (i.e., k = 1)
* Logging of collaborative editing and coding
* Logging of certificate issuance/revocation/renewal
    * This application do not even have multiple parties
* Logging of solar energy consumption/production

Applications where k>1 for consensus.
* We don't really have one. But the coin application used by hyperledger can be one?

## 2. Clear understanding of the properties of our system

### Partition Tolerance

Which level of network partition can we tolerant? 
If we describe this property by toleranting `m` nodes goes offline for `t` seconds, what is the boundary of `m` and `t`?
* `m`
* `t`

### Availability

Can records still be added in a partitioned network?
If so, is there a limitation? i.e., can the partitioned branch be maintained forever and for a huge size, and then merged into the the main DAG?

### Consistency

Which level of consistency can we provide? Is such consistency sufficient to run a crypto currency application?
If we describe this property by the time `t` for the whole network to have the same global view of the DAG at time `t_0`, what is the delta `t - t_0`.
If the delta is infinity, it means there is no global view consistency at all.
If there is a delta, what are the factors?
* timeout for a unvalid record?
* timeout v.s. the partitioned network? 