#!/usr/bin/env bash
timeout 20 pumba --json --log-level info --interval 1s netem --duration 200ms loss --percent 100 containers node_3 2> experiment_1_single_container_fast.json