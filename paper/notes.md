## Impossibility of distributed consensus
The impossibility result shows that no consensus protocol P can ensure that N ≥ 2 processes all agree on the same value for an arbitrary number of registers xi, where i identifies a single register, where in processes communicate by sending messages over the network and assign a value to xi based on the contents of those messages.

## Completeness

**Strong completeness** - every failed process is permanently suspected by *every* correct process

**Weak completeness** - every failed process is only permanently suspected by *some* correct process

One type of completess can emulate the other

## Accuracy

**Strong accuracy** - no process is suspected before it fails

**Weak accuracy** - *some* correct process is never suspected of failure

## Pull vs Push failure detectors 

## Accrual vs binary failure detectors

## Implementatation details 
Push | Adaptive | Binary | Distributed | Sharing | Coarse-grained | Homogeneous | All-to-all | one-to-all propagation
## Glossary:

**failure** - the event in which a process halts without prior notice

**failure detection** - the event in which a failed process is marked as suspected of failure

