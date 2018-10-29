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

 * Heartbeat messages: Push  style - every node broadcasts heartbeats
 * Timeout: Adaptive - with a back-off strategy similar to the one in TCP
 * Interpretation: Binary - node is either alive or not. Don't think we can benefit much from levels of certainty. Maybe I'm wrong here feel free to correct me.
 * Isolation: Sharing(consensus algorithm) 
 * Speciliazitaion: Homogeneous(same failure detector on all nodes)
 * Monitoring type: All-to-all. Given the number of nodes, we will the volume of messages shouldn't be a problem. Maybe will look into neighbourhood-based if needed.
 * Propagation: one-to-all(needs broadcast hardware functionality. If an ability to broadcast is not available I will look into gossip-based propagation.

## Glossary:

**failure** - the event in which a process halts without prior notice

**failure detection** - the event in which a failed process is marked as suspected of failure

