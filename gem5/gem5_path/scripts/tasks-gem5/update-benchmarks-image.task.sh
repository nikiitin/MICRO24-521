
declare_task "update-benchmarks-image" "Build benchmarks and update (or create) the benchmarks disk image. Options:
        --architecture X: Build only architecture X
"

# TODO: Add options to choose what benchmarks should be built.

task_update-benchmarks-image() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    options="$(simpler_getopt "architecture:" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" = "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--" = "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for a in "${archs[@]}" ; do
        update_benchmarks_image "$a"
    done
}

VDS="${SCRIPT_DIR}/../virtual-disk-server"
[[ -x "$VDS" ]] || error_and_exit "virtual-disk-server script not found ($VDS)"

clean_benchmarks_stamp_all() {
    local arch="$1"
    if [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes-native" ]] ; then
        check_stamp_gem5_directory_links
        "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR/make.all")" clean
    fi
}

clean_benchmarks_microbenches_all() {
    local arch="$1"
    if [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes-native" ]] ; then
        check_stamp_gem5_directory_links
        "$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR/make.all")" clean
    fi
}

clean_benchmarks_htmbench_all() {
    local arch="$1"
    if [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "yes" ]] ; then
        check_htmbench_gem5_directory_links
        "$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR/make.all")" clean
    fi
}

update_benchmarks_image_ensure_image_exists() {
    local image_name="$1"
    if [[ ! -f "$image_name" ]] ; then
        echo "$(color green "Disk image not found, creating it. ($image_name)")"
        truncate -s "$BENCHMARKS_DISK_IMAGE_SIZE" "$image_name"
        "$VDS" --img "$image_name" --command 'echo "- - - -" | sfdisk /dev/sdb && mke2fs -j -m0 -L "benchmarks" /dev/sdb1'
    fi
}

update_benchmarks_image() {
    local arch="$1"

    # TODO: make this optional
    # First ensure that the benchmarks are built
    clean_benchmarks_stamp_all "$arch" # clean stamp benchmark before rebuilding to include only the binaries for the desired arch
    build_benchmarks "$arch"

    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"

    update_benchmarks_image_ensure_image_exists "$image_name"
    
    local -a update_libs_cmds=()
    update_libs_cmds=(
        --src "$GEM5_ROOT/util/m5/build/$(get_m5_arch_name "$arch")/out/m5" --copy-to "/mnt/img1p1/benchmarks/" 
        --src "$GEM5_ROOT/gem5_path/benchmarks/libs/" --rsync-to "/mnt/img1p1/libs/" 
        --src "$GEM5_ROOT/tests/test-progs/" --rsync-to "/mnt/img1p1/test-progs/" 
    )

    local -a update_stamp_cmds=()
    if [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes-native" ]] ; then
        update_stamp_cmds=(
            --command "mkdir -p /mnt/img1p1/benchmarks-htm/"
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/stamp/" --rsync-to "/mnt/img1p1/benchmarks-htm/stamp/" 
            --command "/mnt/img1p1/benchmarks-htm/stamp/prepare-inputs" 
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/libs/" --rsync-to "/mnt/img1p1/benchmarks-htm/libs/" 
        )
    elif [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "STAMP benchmarks will not be uploaded because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "STAMP benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_STAMP_ENABLED[$arch] (${BENCHMARKS_STAMP_ENABLED[$arch]})"
    fi

    local -a update_microbenches_cmds=()
    if [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes-native" ]] ; then
        update_microbenches_cmds=(
            --command "mkdir -p /mnt/img1p1/benchmarks-htm/"
            --src "$GEM5_ROOT/tests/test-progs/" --rsync-to "/mnt/img1p1/test-progs/" 
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/microbenches/" --rsync-to "/mnt/img1p1/benchmarks-htm/microbenches/"
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/libs/" --rsync-to "/mnt/img1p1/benchmarks-htm/libs/"
            --src "$GEM5_ROOT/gem5_path/benchmarks/libs/" --rsync-to "/mnt/img1p1/libs/"
        )
    elif [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "MICROBENCHES benchmarks will not be uploaded because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "MICROBENCHES benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_MICROBENCHES_ENABLED[$arch] (${BENCHMARKS_MICROBENCHES_ENABLED[$arch]})"
    fi


    local -a update_htmbench_cmds=()
    if [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "yes-native" ]] ; then
        update_htmbench_cmds=(
            --command "mkdir -p /mnt/img1p1/benchmarks-htm/"
            --src "$GEM5_ROOT/gem5_path/benchmarks/benchmarks-htm/HTMBench/" --rsync-to "/mnt/img1p1/benchmarks-htm/HTMBench/" 
            --src "$GEM5_ROOT/gem5_path/benchmarks/libs/" --rsync-to "/mnt/img1p1/libs/" 
        )
    elif [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "HTMBench benchmarks will not be uploaded because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "HTMBench benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_HTMBENCH_ENABLED[$arch] (${BENCHMARKS_HTMBENCH_ENABLED[$arch]})"
    fi

    local -a update_parsec_cmds=()
    if [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "yes-native" ]] ; then
        local parsec_dir="$(absolute_path "$BENCHMARKS_PARSEC_DIR")"
        update_parsec_cmds=(
            --src "${parsec_dir}/" --rsync-to "/mnt/img1p1/parsec/"
        )
    elif [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "PARSEC benchmarks will not be uploaded because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "PARSEC benchmarks will not be uploaded because they are disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_PARSEC_ENABLED[$arch] (${BENCHMARKS_PARSEC_ENABLED[$arch]})"
    fi
    
    local -a update_splash3_cmds=()
    if [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "yes-native" ]] ; then
        update_splash3_cmds=(
            --src "$GEM5_ROOT/gem5_path/benchmarks/Splash-3/" --rsync-to "/mnt/img1p1/Splash-3/" 
        )
    elif [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "Splash-3 benchmarks will not be uploaded because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "Splash-3 benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_SPLASH3_ENABLED[$arch] (${BENCHMARKS_SPLASH3_ENABLED[$arch]})"
    fi
    
    "$VDS" --img "$image_name" \
           \
           --src "$GEM5_ROOT/util/m5/build/$(get_m5_arch_name "$arch")/out/m5" --copy-to "/mnt/img1p1/benchmarks-htm/" \
           \
           "${update_stamp_cmds[@]}" \
           "${update_microbenches_cmds[@]}" \
           "${update_libs_cmds[@]}" \
           "${update_stamp_cmds[@]}" \
           "${update_htmbench_cmds[@]}" \
           "${update_parsec_cmds[@]}" \
           "${update_splash3_cmds[@]}"
}

