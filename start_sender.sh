#!/bin/bash

export MIN_RTT=1000000

./sender offduration=1 onduration=30000 traffic_params=deterministic,num_cycles=1 cctype=markovian delta_conf=do_ss:auto:0.1 linklog=copa_data_link.log
