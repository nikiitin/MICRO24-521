# All build pre-steps are done in this makefile. It will be called from parent
# folder so DO NOT CALL MAKE HERE!.

# Variables
GEM5_DEPENDENCIES_URL = https://univmurcia-my.sharepoint.com/:u:/g/personal/victor_nicolasc_um_es/EZ4ZYu33sepFsx3oHpQhw2ABQ1OHGb4ZQ8jT77JXG-Objg?e=Tqv8UA&download=1
GEM5_DEPENDENCIES_PATH = ./dependencies
GEM5_DEPENDENCIES_FILES = ("gem5_path/other/qemu-server.img" \
							"gem5_path/x86_64/disks/ubuntu-18-04.img" \
							"gem5_path/x86_64/disks/benchmarks-x86_64.img" \
							"gem5_path/x86_64/binaries/vmlinux-5.4.49")
build_dependencies: check_download_dependencies install
	@echo "All dependencies from gem5 built!"

check_download_dependencies:
# Check if all dependency files are downloaded
	@for file in "$(GEM5_DEPENDENCIES_FILES)"; do \
		if [ ! -f "$$file" ]; then \
			$(MAKE) download_dependencies; \
			break; \
		fi; \
	done

download_dependencies:
	@if [ ! -d "$(GEM5_DEPENDENCIES_PATH)" ]; then \
		mkdir $(GEM5_DEPENDENCIES_PATH); \
	fi
# Check if dependencies.tar.gz exists
	@if [ ! -f "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz" ]; then \
		echo "Downloading dependencies for gem5..."; \
		wget --no-check-certificate "$(GEM5_DEPENDENCIES_URL)" -O "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz"; \
		echo "Dependencies downloaded!"; \
	else \
		echo "Dependencies cached!"; \
	fi

install: unpack_dependencies unpack_modules install_dependencies
	@echo "All gem5 dependencies installed!"

unpack_dependencies:
	@echo "Unpacking dependencies for gem5..."
	@tar -xzvf "$(GEM5_DEPENDENCIES_PATH)/dependencies.tar.gz" -C "$(GEM5_DEPENDENCIES_PATH)"
	@echo "Dependencies successfully unpacked!"

unpack_modules:
	@echo "Unpacking linux modules for ubuntu 20.04 container..."
	@tar -xzvf "$(GEM5_DEPENDENCIES_PATH)/dockerfiles/modules.tar.gz" -C ./dockerfiles
	@echo "Modules unpacked!"
	@echo "Removing tar file..."
	@rm -rf "$(GEM5_DEPENDENCIES_PATH)/dockerfiles"
	@echo "Tar file removed!"

install_dependencies:
	@echo "Installing dependencies for gem5..."
	@rsync -f $(GEM5_DEPENDENCIES_PATH)/gem5_path/* ./gem5_path/
	@echo "Dependencies installed!"