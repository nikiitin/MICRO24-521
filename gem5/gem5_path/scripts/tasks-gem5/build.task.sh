
declare_task "build" "Build all configured GEM5 binaries. Options:
        --architecture X: Build only architecture X
        --protocol X: Build only protocol X
        --build_type X: Build only binary of type X
"

task_build() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    local -a protos=("${ENABLED_PROTOCOLS[@]}")
    local -a build_types=("${ENABLED_BUILD_TYPES[@]}")
    options="$(simpler_getopt "build_type:,protocol:,architecture:" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" == "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--protocol" == "$1" ]] ; then
            shift
            protos=("$1")
        elif [[ "--build_type" == "$1" ]] ; then
            shift
            build_types=("$1")
        elif [[ "--" == "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    for a in "${archs[@]}" ; do
        for p in "${protos[@]}" ; do
            for t in "${build_types[@]}" ; do
                build_gem5 "$a" "$p" "$t"
            done
        done
    done
}

build_gem5() {
    local arch="$1"
    local protocol="$2"
    local build_type="$3"

    pushd "$GEM5_ROOT" > /dev/null
    echo /usr/bin/env python3 $(which scons) -j $(get_num_threads_for_building) $(get_gem5_binary "$arch" "$protocol" "$build_type") "${ADDITIONAL_BUILD_OPTIONS[@]}"
    /usr/bin/env python3 $(which scons) -j $(get_num_threads_for_building) $(get_gem5_binary "$arch" "$protocol" "$build_type") "${ADDITIONAL_BUILD_OPTIONS[@]}"
    popd > /dev/null
}

get_num_threads_for_building() {  
    printf $'1\n%s' "$(($(get_num_processors) - 2))" | sort -nr | head -n1
}

