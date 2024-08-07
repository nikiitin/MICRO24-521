
declare_task "build-benchmarks-virtual" "Build benchmarks in a virtual machine, directly in the benchmarks image. Options:
        --architecture X: Build only architecture X
        --clean-before yes/no: Clean before building (default: no)
"

# TODO: Add options to choose what benchmarks should be built.
BENCHMARKS_STAMP_SELECTED=(
    "bayes"
    "genome"
    "intruder"
    "intruder-no-fsharing"
    "intruder-queuesync"
    "intruder-no-fsharing-queuesync"
    "kmeans"
    "kmeans-queuesync"
    "labyrinth"
    "ssca2"
    "vacation"
    "yada"
)

task_build-benchmarks-virtual() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    local clean_before="no"
    options="$(simpler_getopt "architecture:,clean-before:" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" = "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--clean-before" = "$1" ]] ; then
            shift
            clean_before="$1"
        elif [[ "--" = "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for a in "${archs[@]}" ; do
        build_benchmarks_virtual "$a" "$clean_before"
    done
}

build_benchmarks_virtual() {
    local arch="$1"
    local clean_before="$2"
    
    if [[ "$arch" = "none" ]] ; then 
        build_benchmarks_virtual_sumarray "$arch" "$clean_before"
    else
        # TODO
        echo "$(color yellow "Skipping build of test benchmark (sumarray) in a virtual mechine because it is not yet supported for '$arch'. TODO: fix this")"
    fi

    if [ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes" ] ; then
        echo "$(color yellow "Skipping build of STAMP in a virtual machine because it is not yet supported for '$arch'. TODO: fix this")"
        # TODO
        #build_benchmarks_virtual_stamp "$arch" "$clean_before"
    fi

    if [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        if [[ "$arch" = "aarch64" ]] ; then 
            build_benchmarks_virtual_parsec "$arch" "$clean_before"
        else
            echo "$(color yellow "Skipping build of PARSEC in a virtual machine because it is not yet supported for '$arch'. TODO: fix this")"
        fi
    fi
}

build_benchmarks_virtual_sumarray() {
    local arch="$1"
    local clean_before="$2"

    echo "$(color green "Building test benchmark (sumarray) for $arch")"

    error_and_exit "TODO"
    
    if [[ "$arch" = "x86_64" ]] ; then
        local makefile="Makefile.x86"
        # TODO
    else
        error_and_exit "Architecture $arch not supported for benchmark sumarray"
    fi
    pushd "$GEM5_ROOT/tests/test-progs/caps/sumarray" > /dev/null
    make -f "$makefile"
    popd > /dev/null
}

build_benchmarks_virtual_stamp() {
    local arch="$1"
    local clean_before="$2"

    echo "$(color green "Building stamp benchmarks for $arch in a virtual machine")"
    
    #if [[ "$arch" = "x86_64" ]] ; then
        # TODO
    #elif [[ "$arch" = "aarch64" ]] ; then
	# TODO
    #else
        error_and_exit "Architecture $arch not supported for building stamp in a virtual machine"
    #fi
}

build_benchmarks_virtual_parsec_update_source() {
    local arch="$1"
    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"
    echo "$(color green "Updating sources for parsec benchmarks for $arch")"
    update_benchmarks_image_ensure_image_exists "$image_name"
    local parsec_dir="$(absolute_path "$BENCHMARKS_PARSEC_DIR")"
    local -a update_parsec_cmds=(
        --command "mkdir -p /mnt/img1p1/parsec/"
        --rsync-exclude-from="${parsec_dir}/.gitignore"
        --src "${parsec_dir}/" --rsync-to "/mnt/img1p1/parsec/"
    )
    "$VDS" --img "$image_name" \
           --command "[ -d /mnt/img1p1 ] || { echo \"Could not mount image '$image_name'\" ; exit 1 ; }" \
           \
           "${update_parsec_cmds[@]}"
}

VBS="${SCRIPT_DIR}/../virtual-build-server"
[[ -x "$VBS" ]] || error_and_exit "virtual-build-server script not found ($VBS)"

build_benchmarks_virtual_parsec() {
    local arch="$1"
    local clean_before="$2"

    echo "$(color green "Building parsec benchmarks for $arch in a virtual machine")"
    
    check_parsec_gem5_directory_links
    
    local image_name="$(absolute_path "$(get_benchmarks_disk_image "$arch")")"

    if [[ "$arch" = "aarch64" ]] ; then
        build_benchmarks_virtual_parsec_update_source "$arch"

        local -a compiler_img_commands
        local -a compiler_build_commands
        local scratch_image_name="$(absolute_path "gem5_path/${arch}/disks/build-gcc-tmp.img")"
        if [[ "${BENCHMARKS_PARSEC_COMPILER_VM}" = "system" ]] ; then
            compiler_img_commands=()
            compiler_build_commands=()
        elif [[ "${BENCHMARKS_PARSEC_COMPILER_VM}" = "compile" ]] ; then
            # Building GCC requires a lot of temporary space. Use a scratch disk image for it.
            if [[ -f "$scratch_image_name" ]] ; then
                error_and_exit "Found unexpected «$scratch_image_name»"
            else
                echo "$(color green "Creating scratch image file for building gcc ($scratch_image_name)")."
                truncate -s 12G "$scratch_image_name"
                "$VDS" --img "$scratch_image_name" --command 'echo "- - - -" | sfdisk /dev/sdb && mke2fs -j -m0 -L "buildgcc_tmp" /dev/sdb1'
            fi            
            compiler_img_commands=(
                --img "$scratch_image_name"
            )
            local buildgcc_version="11.3.0"
            local buildgcc_config="buildgcc.native.${arch}.${buildgcc_version}.config"
            compiler_build_commands=(
                --command "cd /benchmarks/parsec ; [ -e ./compiler-environment.sh ] || WORK_DIR='/mnt/img2p1/buildgcc-tmp' ./buildgcc ${buildgcc_config} && ln -s localgcc/gcc-${buildgcc_version}-${arch}-native/env.sh compiler-environment.sh"
            )
        elif [[ "${BENCHMARKS_PARSEC_COMPILER_VM}" = "prebuilt" ]] ; then
            local gcctar="$(absolute_path "${BENCHMARKS_PARSEC_COMPILER_VM_PREBUILT_FILENAME[$arch]}")"
            [[ -f "$gcctar" ]] || error_and_exit "«$gcctar» not found"
            local envshpath="$(tar tf "$gcctar" --wildcards "*/env.sh" | head -n1)"
            [[ -n "$envshpath" ]] || error_and_exit "«$gcctar» does not seem to contain an env.sh."
            echo "$(color green "Installing prebuilt gcc ($gcctar)")."
            "$VDS" --img "$image_name" \
                   --virtfs "$(dirname "$gcctar")" \
                   --command "ln -s /mnt/img1p1/ /benchmarks" \
                   --command "cd /benchmarks/parsec ; [ -e ./compiler-environment.sh ] || { ln -sf 'localgcc/$envshpath' ./compiler-environment.sh ; mkdir -p localgcc ; cd localgcc ; tar xf /mnt/host1/$(basename "$gcctar") ; }"
            compiler_img_commands=()
            compiler_build_commands=()
        else
            error_and_exit "Unknown value for BENCHMARKS_PARSEC_COMPILER_VM (${BENCHMARKS_PARSEC_COMPILER_VM})'"
        fi
        
        local -a clean_before_commands=()
        if [[ "$clean_before" = "yes" ]] ; then
            clean_before_commands=(
                --command "cd /benchmarks/parsec ; ./fullclean"
            )
        fi

        if [[ "$arch" = "aarch64" ]] ; then
            local build_server_type="arm-ubuntu"
        else
            error_and_exit "build_server_type not defined for $arch"
        fi

        "$VBS" --type "$build_server_type" \
               --img "$image_name" \
               "${compiler_img_commands[@]}" \
               --command "[ -d /mnt/img1p1 ] || { echo \"Could not mount image '$image_name'\" ; exit 1 ; }" \
               \
               --command "ln -s /mnt/img1p1/ /benchmarks" \
               "${compiler_build_commands[@]}" \
               "${clean_before_commands[@]}" \
               --command "cd /benchmarks/parsec ; ./parsecmgmt-env -a build -c gcc-hooks -p aarch64_compatible"

        [ -e "$scratch_image_name" ] && rm "$scratch_image_name"
    else
        echo "$(color red "Building PARSEC in a virtual machine not implemented for $arch")"
    fi
}

