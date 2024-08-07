# All build pre-steps are done in this makefile. It will be called from parent
# folder so DO NOT CALL MAKE HERE!.

# Variables
GEM5_MODULES_TAR = gem5/dockerfiles/modules.tar.gz
GEM5_MODULES_PATH = gem5/dockerfiles/modules

build_dependencies: unpack_modules
	@echo "All dependencies from gem5 built!"

unpack_modules:
	@echo "Unpacking linux modules for ubuntu 20.04 container..."
	@tar -xzvf $(GEM5_MODULES_TAR) -C $(GEM5_MODULES_PATH)
	@echo "Modules unpacked!"