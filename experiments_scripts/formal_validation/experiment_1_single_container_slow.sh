#!/usr/bin/env bash
timeout 240 pumba --json --log-level info --interval 20s netem --duration 5s loss --percent 100 containers node_3 2> experiment_1_single_container_slow.json