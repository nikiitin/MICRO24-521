#!/bin/bash

# This script prepares slurm configuration files

TEMPLATES_DIR=../templates
GEM5_DIR=../gem5
SLURM_DIR=../slurm-docker-cluster
TMP_DIR=./tmp_slurm_conf

usage_help() {
  echo "Usage: prepare_configuration.sh -n <number_of_nodes>"
  exit 1
}

check_templates() {
  if { [ ! -f "$TEMPLATES_DIR/slurm.conf.template" ] || \
      [ ! -f "$TEMPLATES_DIR/slurmdbd.conf.template" ] ;}; then
    echo "Not all configuration templates were found in $TEMPLATES_DIR!"
    echo "Please restore the configuration templates."
    exit 1
  fi
}

copy_to_env() {
  cp $TMP_DIR/slurm.conf $SLURM_DIR/slurm.conf
  cp $TMP_DIR/slurmdbd.conf $SLURM_DIR/slurmdbd.conf
  cp $TMP_DIR/slurm.conf $GEM5_DIR/dockerfiles/slurm.conf
  cp $TMP_DIR/slurmdbd.conf $GEM5_DIR/dockerfiles/slurmdbd.conf
}

# Option for number of nodes must be provided
# If no option is provided, print usage and exit
if [ $# -eq 0 ]; then
  usage_help
fi
while getopts ":n:" opt; do
  case ${opt} in
    n )
      NODES=$OPTARG
      # Nodes must be a positive integer
        if ! [[ $NODES =~ ^[0-9]+$ ]]; then
            echo "Number of nodes must be a positive integer"
            usage_help
            exit 1
        fi
      ;;
    \? )
      usage_help
      exit 1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument"
      usage_help
      exit 1
      ;;
  esac
done

# Check that configuration templates exists
check_templates
# Right now it will overwrite everything
# # Check if configuration files already exist in current directory
# if { [ -f ./slurm.conf ] || [ -f ./slurmdbd.conf ] ;}; then
#     echo "Configuration files slurm.conf and slurmdbd.conf already exist in current directory!"
#     echo "Do you want to overwrite them? (y/n)"
#     read -r response
#     if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
#         rm -f ./slurm.conf ./slurmdbd.conf
#     else
#         echo "Exiting ..."
#         exit 1
#     fi
# fi
mkdir "$TMP_DIR"
# Copy the configuration template to this directory
cp "$TEMPLATES_DIR/slurm.conf.template" "$TMP_DIR/slurm.conf"
cp "$TEMPLATES_DIR/slurmdbd.conf.template" "$TMP_DIR/slurmdbd.conf"

# Replace the number of nodes in the configuration file
sed -i "s/NUMBER_OF_NODES/$NODES/g" "$TMP_DIR/slurm.conf"

copy_to_env
# Remove the configuration files from the current directory
# Keep it clean!
rm -rf "$TMP_DIR"