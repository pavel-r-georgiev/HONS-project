#!/usr/bin/env bash
timeout 20 pumba --json --log-level info --interval 1s --random netem --duration 200ms loss --percent 100 containers node_2 node_3 2> experiment_1_multiple_containers_fast.json