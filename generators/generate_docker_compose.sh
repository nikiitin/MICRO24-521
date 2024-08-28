#!/bin/bash

# This script prepares the docker compose file
# To generate the optimal or specified containers for the cluster
# The script will copy the docker-compose.yml file template
# to the current directory and introduce the services

TEMPLATES_DIR=../templates
CONTAINERS_DIR=..
TMP_DIR=./tmp_compose

usage_help() {
  echo "Usage: generate_docker_compose.sh -n <number_of_services> -d <device_name> -w <device_bandwidth> [-k (KVM active)]" 1>&2
  exit 1
}

check_templates() {
  if { [ ! -f "$TEMPLATES_DIR/docker-compose.yml.template" ] ;}; then
    echo "Not all compose templates were found in $TEMPLATES_DIR!"
    echo "Please restore the templates."
    exit 1
  fi
}

copy_to_env() {
  cp $TMP_DIR/docker-compose.yml $CONTAINERS_DIR/docker-compose.yml
}

# Option for number of services must be provided
# If no option is provided, print usage and exit
KVM_FLAG=0
while getopts ":n:d:w:k" opt; do
  case ${opt} in
    n )
      SERVICES=$OPTARG
      # Services must be a positive integer
        if ! [[ $SERVICES =~ ^[0-9]+$ ]]; then
            echo "Number of services must be a positive integer"
            usage_help
            exit 1
        fi
      ;;
    d )
      DEVICE=$OPTARG
      ;;
    w )
      DEVICE_BANDWIDTH=$OPTARG
      if ! [[ $DEVICE_BANDWIDTH =~ ^[0-9]+$ ]]; then
          echo "Device bandwidth must be a positive integer"
          usage_help
          exit 1
      fi
      ;;
    k )
      KVM_FLAG=1
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

# Check that compose templates exists
check_templates

mkdir "$TMP_DIR"
# Copy the compose template
cp "$TEMPLATES_DIR/docker-compose.yml.template" "$TMP_DIR/docker-compose.yml"

# Copy the component template
cp "$TEMPLATES_DIR/service_component.template" "$TMP_DIR/service_component.template"

# Load the component in a variable
SERVICE_COMPONENT=$(<"$TMP_DIR/service_component.template")

SERVICE_STRING=""
SUBSTITUTE_STRING=""
# Prepare a variable with all the required services
for ((i=1; i<=SERVICES; i++)); do
    SUBSTITUTE_STRING=$(sed "s/HOSTNAME/c$i/g" <<< "$SERVICE_COMPONENT")
    # Using pipe to escape the forward slash in the device name
    SUBSTITUTE_STRING=$(sed "s|DEVICENAME|${DEVICE}|g" <<< "$SUBSTITUTE_STRING")
    SUBSTITUTE_STRING=$(sed "s/MAXWRITEBANDWIDTH/${DEVICE_BANDWIDTH}/g" <<< "$SUBSTITUTE_STRING")
    if [ "$KVM_FLAG" -eq 1 ]; then
      SUBSTITUTE_STRING=$(sed "s|DEVICES_KVM|devices:\n     - \"/dev/kvm:/dev/kvm\"|g" <<< "$SUBSTITUTE_STRING")
    else
      SUBSTITUTE_STRING=$(sed "/DEVICES_KVM/d" <<< "$SUBSTITUTE_STRING")
    fi
    SERVICE_STRING+=$(echo -e "\n$SUBSTITUTE_STRING")
done
# Replace the placeholder for kvm devices
if [ "$KVM_FLAG" -eq 1 ]; then
  sed -i "s|DEVICES_KVM|devices:\n     - \"/dev/kvm:/dev/kvm\"|g" "$TMP_DIR/docker-compose.yml"
  sed -i "/DEVICES_KVM/d" "$TMP_DIR/docker-compose.yml"
else
  sed -i "/DEVICES_KVM/d" "$TMP_DIR/docker-compose.yml"
fi
# Replace the placeholder in the compose file with the service string
sed -i "/SLURM_COMPUTE_NODES/r /dev/stdin" "$TMP_DIR/docker-compose.yml" <<< "$SERVICE_STRING"
# Remove the placeholder
sed -i "/SLURM_COMPUTE_NODES/d" "$TMP_DIR/docker-compose.yml"


copy_to_env
# Remove the compose files from the current directory
# Keep it clean!
rm -rf "$TMP_DIR"