generate_slurm:
	@echo "Generating slurm configuration files"
	@echo "Using $(M_NUM_CPUS) CPUs. If you want to " \
		"modify the number of CPUs, check the M_NUM_CPUS " \
		"variable from Makefile."
	@./prepare_slurm_configuration.sh -n $(M_NUM_CPUS)
	@echo "Configuration files generated!"

generate_compose:
	@echo "Generating docker compose file"
	@echo "Will create $(M_NUM_SERVICES). If you want to " \
		"modify the number of CPUs, check the M_NUM_SERVICES " \
		"variable from Makefile."
	@./generate_docker_compose.sh -n $(M_NUM_SERVICES) \
		-d $(M_CURRENT_DEVICE) \
		-w "$(MAX_IO_PER_CONTAINER)" \
		$(KVM_FLAG)

generate_kvm generate_atomic: generate_slurm generate_compose
	@echo "All configuration files generated!"

generate_kvm: KVM_FLAG = "-k" generate

generate_atomic: KVM_FLAG = "" generate