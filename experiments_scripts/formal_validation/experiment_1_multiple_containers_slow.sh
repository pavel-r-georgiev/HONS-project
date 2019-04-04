#!/usr/bin/env bash
timeout 240 pumba --json --log-level info --random --interval 20s netem --duration 5s loss --percent 100 containers node_2 node_3 2> experiment_1_multiple_container.json