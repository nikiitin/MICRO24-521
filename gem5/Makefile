# All build pre-steps are done in this makefile. It will be called from parent
# folder so DO NOT CALL MAKE HERE!.

# Variables
GEM5_PATH = ./gem5
GEM5_DEPENDENCIES_URL = https://univmurcia-my.sharepoint.com/:u:/g/personal/victor_nicolasc_um_es/EZ4ZYu33sepFsx3oHpQhw2ABQ1OHGb4ZQ8jT77JXG-Objg?e=Tqv8UA&download=1
GEM5_DEPENDENCIES_PATH = ./dependencies
build_dependencies: download_dependencies install
	@echo "All dependencies from gem5 built!"

download_dependencies:
	@if [ ! -d "$(GEM5_DEPENDENCIES_PATH)" ]; then \
		mkdir $(GEM5_DEPENDENCIES_PATH); \
	fi
	@echo "Downloading dependencies for gem5..."
	@wget --no-check-certificate "$(GEM5_DEPENDENCIES_URL)" -O "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz"
	@echo "Dependencies downloaded!"


install: unpack_dependencies unpack_modules install_dependencies
	@echo "All gem5 dependencies installed!"

unpack_dependencies:
	@echo "Unpacking dependencies for gem5..."
	@tar -xzvf "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz" -C "$(GEM5_DEPENDENCIES_PATH)"
	@echo "Dependencies successfully unpacked!"
	@echo "Removing tar file..."
	@rm -f "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz"
	@echo "Tar file removed!"

unpack_modules:
	@echo "Unpacking linux modules for ubuntu 20.04 container..."
	@tar -xzvf "$(GEM5_DEPENDENCIES_PATH)/dockerfiles/modules.tar.gz" -C $(GEM5_PATH)/dockerfiles
	@echo "Modules unpacked!"
	@echo "Removing tar file..."
	@rm -f "$(GEM5_DEPENDENCIES_PATH)/dockerfiles/modules.tar.gz"
	@echo "Tar file removed!"

install_dependencies:
	@echo "Installing dependencies for gem5..."
	@mv $(GEM5_DEPENDENCIES_PATH)/gem5_path/* $(GEM5_PATH)/gem5_path/