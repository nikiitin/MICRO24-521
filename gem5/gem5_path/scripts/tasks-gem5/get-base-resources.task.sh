
PATH_IN_ECHO_PREFIX="/home/users/caps/gem5_full_system"
DEFAULT_URL_PREFIX="https://ditec.um.es/~rfernandez/gem5-resources"

RESOURCES_LIST=$( echo "
# arch   type        name                           path_in_echo                                                         url
x86_64   base_image  ubuntu-18-04.img               ${PATH_IN_ECHO_PREFIX}/x86_64/disks/ubuntu-18-04.img                 ${DEFAULT_URL_PREFIX}/x86_64/disks/ubuntu-18-04.2021-11-25.img.xz
aarch64  base_image  ubuntu-18.04-arm64-docker.img  ${PATH_IN_ECHO_PREFIX}/aarch64/disks/ubuntu-18.04-arm64-docker.img  ${DEFAULT_URL_PREFIX}/aarch64/disks/ubuntu-18.04-arm64-docker.2022-05-12.img.xz
x86_64   kernel      vmlinux-5.4.49                 ${PATH_IN_ECHO_PREFIX}/x86_64/binaries/vmlinux-5.4.49                ${DEFAULT_URL_PREFIX}/x86_64/binaries/vmlinux-5.4.49.2021-11-25.xz
aarch64  kernel      vmlinux.arm64                  ${PATH_IN_ECHO_PREFIX}/aarch64/binaries/vmlinux.arm64                ${DEFAULT_URL_PREFIX}/aarch64/binaries/vmlinux.arm64.2021-11-25.xz
aarch64  bootloader  boot_v2.arm64                  ${PATH_IN_ECHO_PREFIX}/aarch64/binaries/boot_v2.arm64                ${DEFAULT_URL_PREFIX}/aarch64/binaries/boot_v2.arm64.2021-11-25.xz
all      qemu-server qemu-server.img                /home/users/caps/qemu-server.img                                     ${DEFAULT_URL_PREFIX}/qemu-server.2023-01-10.img.xz
aarch64  qemu-server qemu-build-server-arm-ubuntu.qcow2   /home/users/caps/qemu-build-server-arm-ubuntu.qcow2            ${DEFAULT_URL_PREFIX}/qemu-build-server-arm-ubuntu.2022-12-20.qcow2.xz
aarch64  qemu-server qemu-build-server-arm-ubuntu.initrd  /home/users/caps/qemu-build-server-arm-ubuntu.initrd           ${DEFAULT_URL_PREFIX}/qemu-build-server-arm-ubuntu.2022-10-10.initrd.xz
aarch64  qemu-server qemu-build-server-arm-ubuntu.kernel  /home/users/caps/qemu-build-server-arm-ubuntu.kernel           ${DEFAULT_URL_PREFIX}/qemu-build-server-arm-ubuntu.2022-10-10.kernel.xz
aarch64  compiler    gcc-11.3.0-aarch64-linux-native.tar.xz  /home/users/caps/gcc-11.3.0-aarch64-linux-native.tar.xz     ${DEFAULT_URL_PREFIX}/gcc-11.3.0-aarch64-linux-native.2023-01-10.tar.xz
" | grep -v '^ *#.*' | grep -v '^ *$' | tr -s ' ')

declare_task "get-base-resources" "Get base resources (disk images, kernels…). Options:
        --architecture X: Get only architecture X
        --overwrite: Overwrite existing files.
        --download: Download resources from the web.
        --use-shared-caps: Use files from ${PATH_IN_ECHO_PREFIX} (default if possible)
        --no-common: Do not download architecture independent resources
"

task_get-base-resources() {
    local -a archs=("${ENABLED_ARCHITECTURES[@]}")
    local overwrite=no
    local mode=auto
    local get_common=yes
    options="$(simpler_getopt "architecture:,overwrite,download,use-shared-echo,no-common" "$@")"
    eval set -- "$options"
    while [[ $# -gt 0 ]] ; do
        if [[ "--architecture" == "$1" ]] ; then
            shift
            archs=("$1")
        elif [[ "--overwrite" == "$1" ]] ; then
            overwrite=yes
        elif [[ "--download" == "$1" ]] ; then
            mode=download
        elif [[ "--use-shared-caps" == "$1" ]] ; then
            mode=link
        elif [[ "--no-common" == "$1" ]] ; then
            get_common=no
        elif [[ "--" == "$1" ]] ; then
            true # ignore
        else 
            error_and_exit "Unknown option '$1'"
        fi
        shift
    done
    if [[ "$mode" == "auto" && -d "$PATH_IN_ECHO_PREFIX" ]] ; then
        echo "$(color blue "Resources will be copied or linked from ${PATH_IN_ECHO_PREFIX}")"
        mode=link
    else
        echo "$(color blue "Resources will be downloaded")"
        mode=download
    fi
    for arch in "${archs[@]}" ; do
        get_base_resources "$arch" "$overwrite" "$mode"
    done
    if [[ "$get_common" != "no" ]] ; then
        echo "Getting architecture independent resources"
        get_base_resource "all" "qemu-server" "qemu-server.img" "gem5_path/other/qemu-server.img" "$mode" "$overwrite"
    fi
}

get_base_resources() {
    local arch="$1"
    local overwrite="$2"
    local mode="$3"
    echo "Getting resources for $arch."

    get_base_resource "$arch" kernel "${ARCH_KERNEL[$arch]}" "$(get_kernel "$arch")" "$mode" "$overwrite"
    get_base_resource "$arch" base_image "${ARCH_BASE_IMAGE[$arch]}" "$(get_base_image "$arch")" "$mode" "$overwrite"
    if [[ -n "${ARCH_BOOTLOADER[$arch]}" ]] ; then 
        get_base_resource "$arch" bootloader "${ARCH_BOOTLOADER[$arch]}" "$(get_bootloader "$arch")" "$mode" "$overwrite"
    fi

    if [ "$arch" = "aarch64" ] ; then
        get_base_resource "$arch" "qemu-server" "qemu-build-server-arm-ubuntu.qcow2" "gem5_path/other/qemu-build-server-arm-ubuntu.qcow2" "$mode" "$overwrite"
        get_base_resource "$arch" "qemu-server" "qemu-build-server-arm-ubuntu.kernel" "gem5_path/other/qemu-build-server-arm-ubuntu.kernel" "$mode" "$overwrite"
        get_base_resource "$arch" "qemu-server" "qemu-build-server-arm-ubuntu.initrd" "gem5_path/other/qemu-build-server-arm-ubuntu.initrd" "$mode" "$overwrite"
        get_base_resource "$arch" "compiler" "gcc-11.3.0-${arch}-linux-native.tar.xz" "gem5_path/${arch}/other/gcc-11.3.0-${arch}-linux-native.tar.xz" "$mode" "$overwrite"
    fi
} 

check_base_resource_known() {
    local arch="$1"
    local type="$2"
    local name="$3"
    echo "$RESOURCES_LIST" | grep -q "^${1} ${2} ${3} "
}

get_base_resource_path_in_echo() {
    local arch="$1"
    local type="$2"
    local name="$3"
    echo "$RESOURCES_LIST" | grep "^${1} ${2} ${3} " | cut -d' ' -f4
}

get_base_resource_url() {
    local arch="$1"
    local type="$2"
    local name="$3"
    echo "$RESOURCES_LIST" | grep "^${1} ${2} ${3} " | cut -d' ' -f5
}

get_base_resource() {
    local arch="$1"
    local type="$2"
    local name="$3"
    local destination="$4" # relative to $GEM5_ROOT
    local mode="$5"
    local overwrite="$6"

    if [[ -f "$(absolute_path "$destination")" && "$overwrite" != "yes" ]] ; then
        echo "$(color yellow "'$destination' already exists. Will not overwrite it.")"
    else
        if check_base_resource_known "$arch" "$type" "$name" ; then
            rm -f "$GEM5_ROOT/$destination" # remove the destination if it exists. This is important if it is a symbolic link to avoid accidentally overwritting the destination. Also it is important to avoid resolving the link before removing it.
            mkdir -p "$(dirname "$GEM5_ROOT/$destination")"
            if [[ "$mode" == "link" ]] ; then
                local src="$(get_base_resource_path_in_echo "$arch" "$type" "$name")"
                echo "Linking '$src' to '$GEM5_ROOT/$destination'"
                ln -s "$src" "$GEM5_ROOT/$destination"
            elif [[ "$mode" == "download" ]] ; then
                local src="$(get_base_resource_url "$arch" "$type" "$name")"
                echo "Downloading '$src' to '$GEM5_ROOT/$destination'"
                download "$src" "$GEM5_ROOT/$destination"
            else
                error_and_exit "Mode '$mode' unknown."
            fi
        else
            error_and_exit "Resource '$name' of type $type for architecture '$arch' not known."
        fi
    fi
}

download() {
    local src="$1"
    local dst="$2"
    if [[ "$src" =~ .xz$ ]] ; then
        curl "$src" | unxz > "$dst"
    else
        curl "$src" > "$dst"
    fi || { rm -f "$dst" ; error_and_exit "Cannot download '$src'." ; }
}
