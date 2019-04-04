#!/usr/bin/env bash
pumba --json --log-level info --interval 1500ms netem --duration 500ms loss --percent 100 containers node_3
