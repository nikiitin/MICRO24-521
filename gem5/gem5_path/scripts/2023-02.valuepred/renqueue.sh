#!/bin/bash


FAILED_LIST=$(../check_simulations.sh ~/workspace/sims/ruben/gem5/results/valuepred-retry-sens | grep "FAILED")
echo "$FAILED_LIST"
FAILED_LIST=$(echo "$FAILED_LIST" | tr -s " " | cut -d " " -f2)
FAILED_LIST=$(echo "$FAILED_LIST" | tr -d "\t")
while read -r line
do
	"$line/enqueue"
done <<< $FAILED_LIST
