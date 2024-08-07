
declare_task "create-boot-checkpoints" "Build all configured boot checkpoints. Options:
        --architecture X: Build only architecture X
        --num_cpus X: Build only for X cpus
        --overwrite: Overwrite existing checkpoints.
        --skip-existing: Generate only missing checkpoints.
        --disable-kvm: Do not use KVM even if available.
"

task_create-boot-checkpoints() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    local -a ncpus=("${ENABLED_NUM_CPUS[@]}")
    local overwrite=no
    local skip_existing=no
    local disable_kvm=no
    options="$(simpler_getopt "num_cpus:,architecture:,overwrite,skip-existing,disable-kvm" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" == "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--num_cpus" == "$1" ]] ; then
            shift
            ncpus=("$1")
        elif [[ "--overwrite" == "$1" ]] ; then
            overwrite=yes
        elif [[ "--skip-existing" == "$1" ]] ; then
            skip_existing=yes
        elif [[ "--disable-kvm" == "$1" ]] ; then
            disable_kvm=yes
        elif [[ "--" == "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for arch in "${archs[@]}" ; do
        for num_cpus in "${ncpus[@]}" ; do
            create_boot_checkpoint "$arch" "$num_cpus" "$overwrite" "$skip_existing" "$disable_kvm"
        done
    done
}

# returns the path relative to $GEM5_ROOT to the booted checkpoint for a given architecture and number of processors
get_boot_checkpoint_dir() {
    local arch="$1"
    local num_cpus="$2"
    echo "gem5_path/$arch/checkpoints/booted/${num_cpus}_cores" # simulate.sh.common requires this filename
}

create_boot_checkpoint() {
    local arch="$1"
    local num_cpus="$2"
    local overwrite="$3"
    local skip_existing="$4"
    local disable_kvm="$5"
    if [[ "$overwrite" == "yes" && "$skip_existing" == "yes" ]] ; then
        error_and_exit "--overwrite and --skip-existing cannot be specified at the same time."
    fi

    local gem5="$(absolute_path "$(find_built_gem5_binary_for_arch "$arch")")"
    if [[ "$disable_kvm" != "yes" && "$arch" == "x86_64" ]] ; then
        local cpu_type="X86KvmCPU"  # Use KVM if possible, currently only available for x86_64
    else
        local cpu_type="AtomicSimpleCPU" # Otherwise, use AtomicSimpleCPU
    fi
    local kernel="$(absolute_path "$(get_kernel "$arch")")"
    ensure_file_exists "$kernel" "Kernel binary"
    local disk_image="$(absolute_path "$(get_base_image "$arch")")"
    ensure_file_exists "$disk_image" "Base system disk image"
    local benchmarks_image="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"
    ensure_file_exists "$benchmarks_image" "Benchmarks disk image"
    local bootscript="$(absolute_path ${BOOTSCRIPT})"
    ensure_file_exists "$bootscript" "Boot script"
    if [[ "$(get_bootloader "$arch")" != "" ]] ; then
        local bootloader="$(absolute_path "$(get_bootloader "$arch")")"
        ensure_file_exists "$bootloader" "Bootloader"
    fi

    local output_dir="$(absolute_path "$(get_boot_checkpoint_dir "$arch" "$num_cpus")")"    
    if [[ -d "$output_dir" && "$overwrite" == "yes" ]] ; then
        echo "Removing previously existing '$output_dir'."
        rm -rf "$output_dir"
    fi
    if [[ -d "$output_dir" && "$skip_existing" == "yes" ]] ; then
        echo "$(color green "Skipping previously existing '$output_dir'.")"
    elif [[ -d "$output_dir" && "$skip_existing" == "no" ]] ; then
        error_and_exit "Will not overwrite previously existing '$output_dir' (see --skip-existing and --overwrite options)."
    else
        echo "Creating booted checkpoint for $arch with $num_cpus cpus at '$output_dir'."
        mkdir -p "$output_dir"
        "${gem5}" \
	    --outdir="${output_dir}" \
	    --redirect-stdout --redirect-stderr \
	    "${GEM5_ROOT}/configs/example/fs.py" \
	    ${ARCH_EXTRA_OPTIONS[$arch]} \
	    --num-cpus="${num_cpus}" \
	    --mem-size="${ARCH_MEMORY[$arch]}" \
	    --cpu-type="${cpu_type}" \
	    --checkpoint-dir="${output_dir}" \
	    --kernel="${kernel}" \
            ${bootloader:+"--bootloader=$bootloader"} \
	    --disk-image="${disk_image}" \
	    --root-device="${ARCH_ROOT_DEVICE[$arch]}" \
	    --disk-image="${benchmarks_image}" \
	    --script="${bootscript}"
    fi
}

# search for any already built gem5 binary among the enabled configurations
# returns path relative to GEM5_ROOT
find_built_gem5_binary_for_arch() {
    local arch="$1"
    local -a build_types=("${ENABLED_BUILD_TYPES[@]}")
    # prioritize opt and fast if enabled
    if list_contains fast "${build_types[@]}" ; then
        build_types=(fast $(remove_from_list fast "${build_types[@]}"))
    fi
    if list_contains opt "${build_types[@]}" ; then
        build_types=(opt $(remove_from_list opt "${build_types[@]}"))
    fi
    local -a tried=()
    local found=no
    for proto in "${ENABLED_PROTOCOLS[@]}" ; do
        for bt in "${build_types[@]}" ; do
            if [[ "$found" != "yes" ]] ; then
                local b="$(get_gem5_binary "$arch" "$proto" "$bt")"
                if [[ -x "$(absolute_path "$b")" ]] ; then
                    echo "$b"
                    found=yes
                else
                    tried=("${tried[@]}" "$b")
                fi
            fi
        done
    done
    if [[ "$found" != "yes" ]] ; then
        error_and_exit "No binary suitable for $arch found. Tried:"$'\n'"$(for t in "${tried[@]}" ; do echo "  $t" ; done)"
    fi
}
