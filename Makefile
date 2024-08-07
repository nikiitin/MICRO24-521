DOCKER_ENGINE_VERSION = 27.1.1
DOCKER_COMPOSE_VERSION = 2.29.1
CONTAINERD_VERSION = 1.7.19
# Maximum memory per container in kbytes
# Set to 3.5GiB by default
# Should not expect to spend more than 3.5GiB to execute
# gem5 simulations
MAX_MEM_PER_CONTAINER = 3670016
# Limit the I/O for all containers to 1GB
MAX_IO = 1000000000
M_NUM_CPUS = $(shell grep -c ^processor /proc/cpuinfo)
M_MAX_MEM = $(shell cat /proc/meminfo | grep MemTotal | awk '{print $$2}')
MAXIMUM_ALLOCATABLE_MEM_NODES = $(shell echo $$(($(M_MAX_MEM) / $(MAX_MEM_PER_CONTAINER))))
# Maximum number of services that can be run in parallel
# It is limited either by the number of CPUs or the number of allocatable memory nodes
# The minimum between the two is the maximum number of services
M_NUM_SERVICES = $(shell echo $$(($(M_NUM_CPUS) < $(MAXIMUM_ALLOCATABLE_MEM_NODES) ? $(M_NUM_CPUS) : $(MAXIMUM_ALLOCATABLE_MEM_NODES))))
ifeq ($(M_NUM_SERVICES), 0)
	M_NUM_SERVICES = 1
endif
# Get the maximum bandwidth for disk for each container
MAX_IO_PER_CONTAINER = $(shell echo $$(($(MAX_IO) / $(M_NUM_SERVICES))))
# Check if results directory exists
# If not, create it
ifneq ($(wildcard ./results),)
else
	$(shell mkdir results)
endif
# Get the device the results directory is in
M_CURRENT_DEVICE = $(shell lsblk -no pkname $(shell df ./results | awk '{print $$1}' | tail -n1))
export MAX_IO_PER_CONTAINER
export M_CURRENT_DEVICE
export M_NUM_CPUS
export M_NUM_SERVICES
### KVM ENVIROMENT ###
check_kvm_environment: kvm_check reduce_paranoid
	@echo "KVM environment is ready!"

kvm_check: 
# Check if KVM is installed
	@echo "Checking if KVM is installed..."
	@lsmod | grep kvm
# If installed, finish
# If not installed, ask user if wants to install KVM
# If yes, install KVM
# If no, exit
	@if [ $$? -ne 0 ]; then \
		echo "KVM is not installed. Do you want to install KVM? [y/n]"; \
		read -r answer; \
		if [ "$$answer" = "y" ]; then \
			make kvm_install; \
		else \
			echo "Exiting..."; \
			exit 1; \
		fi; \
	fi

reduce_paranoid:
# Check the current paranoid level from /proc/sys/kernel/perf_event_paranoid
# If the level is 1 or less, finish
# If the level is more than 1, ask the user if wants to reduce the level
# If yes, reduce the level
# If no, exit
	@echo "Checking the current paranoid level..."
	@CURRENT_PARANOID_LEVEL=$(shell cat /proc/sys/kernel/perf_event_paranoid); \
	echo "Current paranoid level: $$CURRENT_PARANOID_LEVEL"; \
	if [ $$CURRENT_PARANOID_LEVEL -gt 1 ]; then \
		echo "The current paranoid level is higher than 1. \
		Do you want to reduce the paranoid level to 1? [y/n]"; \
		read -r answer; \
		if [ "$$answer" = "y" ]; then \
			sudo sh -c 'echo 1 >/proc/sys/kernel/perf_event_paranoid'; \
		else \
			echo "Exiting..."; \
			exit 1; \
		fi; \
	fi
	

### DOCKER ENVIROMENT ###
check_docker_environment:
	@echo "Checking Docker environment..."
	@make docker_check
	@make docker_compose_check



docker_check: docker_installed docker_version
	@echo "Docker ver.$(DOCKER_ENGINE_VERSION) found!"


docker_installed:
	@echo "Checking if Docker is installed..."
	@which docker
# If installed, finish
# If not installed, ask user if wants to install Docker
# If yes, install Docker
# If no, exit
	@if [ $$? -ne 0 ]; then \
		echo "Docker is not installed. Do you want to install Docker? [y/n]"; \
		read -r answer; \
		if [ "$$answer" = "y" ]; then \
			make docker_install; \
		else \
			echo "Exiting..."; \
			exit 1; \
		fi; \
	fi

docker_version:
	@echo "Checking Docker version..."
# Get Docker version
# Then, check if the version is the required one
# Note that everything must be in the same shell command
	@CURRENT_DOCKER_VERSION=$(shell docker --version | awk '{print $$3}' | sed 's/,//g'); \
	echo "Current Docker version: $$CURRENT_DOCKER_VERSION"; \
	if [ "$$CURRENT_DOCKER_VERSION" != "$(DOCKER_ENGINE_VERSION)" ]; then \
		echo "Docker version do not match the required one. \
		Please update Docker to version $(DOCKER_ENGINE_VERSION)"; \
		exit 1; \
	fi

docker_install:
	@echo "Installing Docker... Will sudo commands to install Docker."
	@sudo apt-get update
# Need to install curl and ca-certificates to download Docker keyrings
	@sudo apt-get install -y ca-certificates curl
# Install Docker keyrings
	@sudo install -m 0755 -d /etc/apt/keyrings
	@sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
	@sudo chmod a+r /etc/apt/keyrings/docker.asc
# Add Docker repository
	@echo "deb [signed-by=/etc/apt/keyrings/docker.asc] \
		https://download.docker.com/linux/ubuntu \
		$(lsb_release -cs) stable" | \
		sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
	@sudo apt-get update
# Install Docker
	@sudo apt-get install -y docker-ce:$(DOCKER_ENGINE_VERSION) \
		docker-ce-cli:$(DOCKER_ENGINE_VERSION) \
		containerd.io:$(CONTAINERD_VERSION)
	@echo "Docker installed successfully."

docker_compose_check: docker_compose_installed docker_compose_version
	@echo "Docker compose ver.$(DOCKER_COMPOSE_VERSION) found!"

docker_compose_installed:
	@echo "Checking if Docker Compose is installed..."
	@docker compose > /dev/null 2>&1
# Same than docker
	@if [ $$? -ne 0 ]; then \
		echo "Docker Compose is not installed. Do you want to install Docker Compose? [y/n]"; \
		read -r answer; \
		if [ "$$answer" = "y" ]; then \
			make docker_compose_install; \
		else \
			echo "Exiting..."; \
			exit 1; \
		fi; \
	fi

docker_compose_version:
	@echo "Checking Docker Compose version..."
	@CURRENT_DOCKER_COMPOSE_VERSION=$(shell docker compose version | awk '{print $$4}' | sed 's/v//g'); \
	echo "Current Docker Compose version: $$CURRENT_DOCKER_COMPOSE_VERSION"; \
	if [ "$$CURRENT_DOCKER_COMPOSE_VERSION" != "$(DOCKER_COMPOSE_VERSION)" ]; then \
		echo "Docker Compose version do not match the required one. \
		Please update Docker Compose to version $(DOCKER_COMPOSE_VERSION)"; \
		exit 1; \
	fi

docker_compose_install:
	@echo "Installing Docker Compose..."
	@sudo apt install -y docker-compose:$(DOCKER_COMPOSE_VERSION) \
		docker-compose-plugin:$(DOCKER_COMPOSE_VERSION)

### END DOCKER ENVIROMENT ###
### CONTAINER CONFIGURATION ###

configure_containers:
	@cd generators && make generate
	@echo "Containers configured!"

configure_dependencies_gem5:
	@cd gem5 && make build_dependencies
	@echo "gem5 dependencies installed!"
### END CONTAINER CONFIGURATION ###


build: check_kvm_environment check_docker_environment configure_containers configure_dependencies_gem5
	@echo "Building all the containers from the compose file..."
	@echo "This may take a while and a lot of disk space..."
	@echo "Additionally, it requires sudo permissions."
	@sudo docker compose build
	@echo "Everything built correctly!"
	make notes
	

notes:
	@echo "Take into account that you may need to run the containers with sudo."
	@echo "To run the containers, execute: make run"
	@echo "To stop the containers, execute: make stop"
	@echo "To clean the containers, execute: make clean"
	@echo "FYI: There seems to be a bug that consumes a lot of space from disk."
	@echo "This space is taken from the /var/lib/docker directory."
	@echo "NOTE: KVM is required to be installed in your machine as" \
		"gem5 uses KVM to run the simulations with fast-forward." \
		"all the containers will try to use /dev/kvm device from this host."

run:
	@echo "Running the containers..."
	@sudo docker compose up -d
	@echo "Containers running!"

stop:
	@echo "Stopping the containers..."
	@sudo docker compose down
	@echo "Containers stopped!"

connect:
	@echo "Connecting to the slurm controller as root user..."
	@sudo docker exec -it slurmctld /bin/bash

clean:
	@echo "Removing all the containers..."
	@sudo docker compose down
	@sudo docker image prune --filter="label=image_label=micro2024_521" --force
	@echo "To totally clean the environment, I'd suggest to remove everything from /var/lib/docker directory."
