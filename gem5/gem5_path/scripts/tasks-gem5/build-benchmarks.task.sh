
declare_task "build-benchmarks" "Build benchmarks in the host system. Options:
        --architecture X: Build only architecture X
        BUG: Due to the way that STAMP becnhmarks are built, only one architecture can be built each time.
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

BENCHMARKS_HTMBENCH_SELECTED=(
    "avl_tree"
    "berkely-db"
    "bplus-tree"
    "parsec-2.1/pkgs/kernels/dedup"
    "parsec-2.1/pkgs/kernels/dedup-cp"
)

BENCHMARKS_SPLASH3_SELECTED=(
    "apps/barnes"
    "apps/fmm"
    "apps/ocean/contiguous_partitions"
    "apps/ocean/non_contiguous_partitions"
    "apps/radiosity"
    "apps/raytrace"
    "apps/volrend"
    "apps/water-nsquared"
    "apps/water-spatial"
    "kernels/cholesky"
    "kernels/fft"
    "kernels/lu/contiguous_blocks"
    "kernels/lu/non_contiguous_blocks"
    "kernels/radix"
)

BENCHMARKS_MICROBENCHES_SELECTED=(
    "llbenchsimple"
    "clusteradding"
)

task_build-benchmarks() {
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
        build_benchmarks "$a"
    done
}

build_benchmarks() {
    local arch="$1"

    if [[ "$arch" = "x86_64" ]] ; then 
        build_benchmarks_sumarray "$arch"
    else
        # TODO
        echo "$(color yellow "Skipping build of test benchmark (sumarray) because it is not yet supported for '$arch'. TODO: fix this")"
    fi

    if [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes-native" ]] ; then
        build_benchmarks_stamp "$arch"
    elif [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "STAMP benchmarks will not be built because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_STAMP_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "STAMP benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_STAMP_ENABLED[$arch] (${BENCHMARKS_STAMP_ENABLED[$arch]})"
    fi

    if [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes" || "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes-native" ]] ; then
        build_benchmarks_microbenches "$arch"
    elif [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "microbenches will not be built because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_MICROBENCHES_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "microbenches disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_MICROBENCHES_ENABLED[$arch] (${BENCHMARKS_MICROBENCHES_ENABLED[$arch]})"
    fi

    if [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "yes-native" ]] ; then
        build_benchmarks_htmbench "$arch"
    elif [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "HTMBENCH benchmarks will not be built because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_HTMBENCH_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "HTMBENCH benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_HTMBENCH_ENABLED[$arch] (${BENCHMARKS_HTMBENCH_ENABLED[$arch]})"
    fi

    if [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "yes-native" ]] ; then
        build_benchmarks_splash3 "$arch"
    elif [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "SPLASH3 benchmarks will not be built because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_SPLASH3_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "SPLASH3 benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_SPLASH3_ENABLED[$arch] (${BENCHMARKS_SPLASH3_ENABLED[$arch]})"
    fi

    if [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "yes-native" ]] ; then
        build_benchmarks_parsec "$arch"
    elif [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "yes-virtual" ]] ; then
        echo "$(color green "PARSEC benchmarks will not be built because they are built directly in the image for $arch.")"
    elif [[ "${BENCHMARKS_PARSEC_ENABLED[$arch]}" = "no" ]] ; then
        echo "$(color green "PARSEC benchmarks disabled for $arch.")"
    else
        error_and_exit "Invalid value for BENCHMARKS_PARSEC_ENABLED[$arch] (${BENCHMARKS_PARSEC_ENABLED[$arch]})"
    fi
}

build_benchmarks_sumarray() {
    local arch="$1"

    echo "$(color green "Building test benchmark (sumarray) for $arch")"
    
    if [[ "$arch" = "x86_64" ]] ; then
        local makefile="Makefile.x86"
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for benchmark sumarray"
    fi
    pushd "$GEM5_ROOT/tests/test-progs/caps/sumarray" > /dev/null
    make -f "$makefile"
    popd > /dev/null
}

build_benchmarks_stamp() {
    local arch="$1"

    echo "$(color green "Building stamp benchmarks for $arch")"
    
    if [[ "$arch" = "x86_64" ]] ; then
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    elif [[ "$arch" = "aarch64" ]] ; then
        export AARCH64_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for stamp"
    fi

    check_stamp_gem5_directory_links
    
    for b in "${BENCHMARKS_STAMP_SELECTED[@]}" ; do
        for s in "${BENCHMARKS_STAMP_FLAVOURS[@]}" ; do
            if [[ "$arch" = "aarch64" && "$s" = "htm.fallbacklock2phase" ]] ; then
                echo "$(color yellow "Skipping build of $b.$a.$s (TODO)")"
            else
                (
                    echo "$(color green "Build $b.$a.$s")"
                    cd "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR/$b")"
                    make -j $(get_num_threads_for_building) -f "Makefile.$s" "ARCH=$arch"
                )
            fi
        done
    done
}

check_stamp_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_HTM_STAMP_DIR}" ]] ; then
        error_and_exit "Stamp directory symlink '$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR")")', or disable these benchmarks (BENCHMARKS_STAMP_ENABLED[*]=no)."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR")/gem5" ]] ; then
        ln -s "$GEM5_ROOT" "$(absolute_path "$BENCHMARKS_HTM_STAMP_DIR")/gem5"
    fi
}

build_benchmarks_htmbench() {
    local arch="$1"

    echo "$(color green "Building HTMBench benchmarks for $arch")"

    if [[ "$arch" = "x86_64" ]] ; then
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    elif [[ "$arch" = "aarch64" ]] ; then
        export AARCH64_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for htmbench"
    fi
    
    check_htmbench_gem5_directory_links
    
    for b in "${BENCHMARKS_HTMBENCH_SELECTED[@]}" ; do
        for h in "${BENCHMARKS_HTMBENCH_FLAVOURS[@]}" ; do
            (
                echo "$(color green "Build $b ARCH=$a HANDLER=$h")"
                cd "$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR/benchmark/$b")"
                make -j $(get_num_threads_for_building) "ARCH=$arch" "HANDLER=$h"
            )
        done
    done
}

build_benchmarks_microbenches() {
    local arch="$1"

    echo "$(color green "Building microbenches for $arch")"
    
    if [[ "$arch" = "x86_64" ]] ; then
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    elif [[ "$arch" = "aarch64" ]] ; then
        export AARCH64_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for microbenches"
    fi

    check_microbenches_gem5_directory_links
    
    for b in "${BENCHMARKS_MICROBENCHES_SELECTED[@]}" ; do
        for s in "${BENCHMARKS_MICROBENCHES_FLAVOURS[@]}" ; do
            if [[ "$arch" = "aarch64" && "$s" = "htm.fallbacklock2phase" ]] ; then
                echo "$(color yellow "Skipping build of $b.$a.$s (TODO)")"
            else
                (
                    echo "$(color green "Build $b.$a.$s")"
                    cd "$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR/$b")"
                    make -j $(get_num_threads_for_building) -f "Makefile.$s" "ARCH=$arch"
                )
            fi
        done
    done
}

check_microbenches_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_HTM_MICROBENCHES_DIR}" ]] ; then
        error_and_exit "Microbenches directory symlink '$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it '$(dirname "$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR")")'."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_MICROBENCHES_DIR")/gem5-libs" ]] ; then
        local GEM5_LIBS_DIR="${GEM5_ROOT}/gem5_path/benchmarks/libs/"
        ln -s "$(realpath --relative-to="$(absolute_path "${BENCHMARKS_HTM_MICROBENCHES_DIR}")" "$GEM5_LIBS_DIR")" "$(absolute_path "${BENCHMARKS_HTM_MICROBENCHES_DIR}")/gem5-libs"
    fi
}

check_htmbench_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_HTM_HTMBENCH_DIR}" ]] ; then
        error_and_exit "HTMBench directory symlink '$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR")")', or disable these benchmarks (BENCHMARKS_HTMBENCH_ENABLED[*]=no)."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_HTM_HTMBENCH_DIR")/gem5-libs" ]] ; then
        local GEM5_LIBS_DIR="${GEM5_ROOT}/gem5_path/benchmarks/libs/"
        ln -s "$(realpath --relative-to="$(absolute_path "${BENCHMARKS_HTM_HTMBENCH_DIR}")" "$GEM5_LIBS_DIR")" "$(absolute_path "${BENCHMARKS_HTM_HTMBENCH_DIR}")/gem5-libs"
    fi
}

build_benchmarks_splash3() {
    local arch="$1"

    echo "$(color green "Building Splash3 benchmarks for $arch")"

    if [[ "$arch" = "x86_64" ]] ; then
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    elif [[ "$arch" = "aarch64" ]] ; then
        export AARCH64_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for splash3"
    fi

    check_splash3_gem5_directory_links

    for b in "${BENCHMARKS_SPLASH3_SELECTED[@]}" ; do
        (
            echo "$(color green "Build $b ARCH=$a")"
            cd "$(absolute_path "$BENCHMARKS_SPLASH3_DIR/codes/$b")"
            make -j $(get_num_threads_for_building) "ARCH=$arch"
        )
    done
}

check_splash3_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_SPLASH3_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_SPLASH3_DIR}" ]] ; then
        error_and_exit "Splash3 directory symlink '$(absolute_path "$BENCHMARKS_SPLASH3_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_SPLASH3_DIR")")', or disable these benchmarks (BENCHMARKS_SPLASH3_ENABLED[*]=no)."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_SPLASH3_DIR")/codes/gem5-libs" ]] ; then
        local GEM5_LIBS_DIR="${GEM5_ROOT}/gem5_path/benchmarks/libs/"
        ln -s "$(realpath --relative-to="$(absolute_path "${BENCHMARKS_SPLASH3_DIR}/codes")" "$GEM5_LIBS_DIR")" "$(absolute_path "${BENCHMARKS_SPLASH3_DIR}")/codes/gem5-libs"
    fi
}

build_benchmarks_parsec() {
    local arch="$1"

    echo "$(color green "Building parsec benchmarks for $arch")"
    
    if [[ "$arch" = "x86_64" ]] ; then
        export X86_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    elif [[ "$arch" = "aarch64" ]] ; then
        export AARCH64_CROSS_GCC_PREFIX="${BENCHMARKS_ARCH_COMPILER_PREFIX[$arch]}"
    else
        error_and_exit "Architecture $arch not supported for parsec"
    fi

    check_parsec_gem5_directory_links
    
    if [[ "$arch" = "x86_64" ]] ; then
        (
            echo "$(color green "Build PARSEC for $arch")"
            cd "$(absolute_path "$BENCHMARKS_PARSEC_DIR")"
            ./parsecmgmt-env -a build -p all -c gcc-hooks
        )
    else
        echo "$(color red "Building PARSEC not implemented for $arch (try tasks-gem5 build-benchmarks-virtual)")"
    fi
}

check_parsec_gem5_directory_links() {
    if [[ ! -d "$(absolute_path "$BENCHMARKS_PARSEC_DIR")" || ! -L "${GEM5_ROOT}/${BENCHMARKS_PARSEC_DIR}" ]] ; then
        error_and_exit "Parsec directory symlink '$(absolute_path "$BENCHMARKS_PARSEC_DIR")' not found. Clone the repository in a directory out of ${GEM5_ROOT} and create a symbolic link to it in '$(dirname "$(absolute_path "$BENCHMARKS_PARSEC_DIR")")', or disable these benchmarks (BENCHMARKS_PARSEC_ENABLED[*]=no)."
    fi

    if [[ ! -d "$(absolute_path "$BENCHMARKS_PARSEC_DIR")/gem5-libs" ]] ; then
        GEM5_LIBS_DIR="${GEM5_ROOT}/gem5_path/benchmarks/libs/"
        ln -s "$(realpath --relative-to="$(absolute_path "${BENCHMARKS_PARSEC_DIR}")" "$GEM5_LIBS_DIR")" "$(absolute_path "${BENCHMARKS_PARSEC_DIR}")/gem5-libs"
    fi
}

