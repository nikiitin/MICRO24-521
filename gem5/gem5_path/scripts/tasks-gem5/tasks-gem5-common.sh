# -*- sh -*-

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
SCRIPT_COMMAND="$0"
set -o nounset
set -o pipefail
set -o errexit
trap 'echo "$SCRIPT_COMMAND: error $? at line $LINENO"' ERR


# Set GEM5_ROOT assuming that this script is in ${GEM5_ROOT}/gem5_path/scripts/tasks-gem5/
GEM5_ROOT="$(realpath "$SCRIPT_DIR/../../..")"

# returns an absolute path, resolving relative paths from $GEM5_ROOT
absolute_path() {
    local p="$1"
    if [[ "$p" =~ ^/.* ]] ; then
        realpath -m "$p" # already absolute
    else
        realpath -m "${GEM5_ROOT}/$p"
    fi
}

# Error reporting
error_and_exit() {
    echo "$(color red "$1")" 1>&2
    exit 1
}

ensure_file_exists() {
    local f="$1"
    local desc="$2"
    if [[ ! -f "$f" ]] ; then
        error_and_exit "File not found: '$f' ($desc)"
    fi
}

## List of tasks and information about them 

declare -a TASKS     # Array of all known tasks
declare -A TASK_HELP # Map of documentation strings

declare_task() {
    local task_name="$1"
    local task_doc="$2"
    if is_known_task "$task_name" ; then
        error_and_exit "Duplicated task name: $task_name"
    else
        TASK_HELP["$task_name"]="$task_doc"
        TASKS=(${TASKS[@]} "$task_name")
    fi                           
}

is_known_task() {
    for t in ${TASKS[@]} ; do echo "$t" ; done | grep -q -e "^$1\$"
}


## Functions to locate some files and other configuration queries

# returns the path relative to $GEM5_ROOT to the base disk image for a given architecture
get_base_image() {
    local arch="$1"
    echo "gem5_path/$arch/disks/${ARCH_BASE_IMAGE[$arch]}"
}

# returns the path relative to $GEM5_ROOT to the kernel for a given architecture
get_kernel() {
    local arch="$1"
    echo "gem5_path/$arch/binaries/${ARCH_KERNEL[$arch]}"
}

# returns the path relative to $GEM5_ROOT to the bootloader for a given architecture, or "" if there is no bootloader
get_bootloader() {
    local arch="$1"
    local b="${ARCH_BOOTLOADER[$arch]}"
    if [[ "$b" == "" ]] ; then
        echo "$b"
    else
        echo "gem5_path/$arch/binaries/$b"
    fi
}

# returns the path relative to $GEM5_ROOT to the becnhamrks disk image for a given architecture
get_benchmarks_disk_image() {
    local arch="$1"
    echo "gem5_path/${arch}/disks/benchmarks-${arch}.img"
}

# Get the arch name used by GEM5 (i.e, X86 instead of x86_64)
get_gem5_arch_name() {
    local arch="$1"
    if [[ "$arch" == "x86_64" ]] ; then
        echo "X86"
    elif [[ "$arch" == "aarch64" ]] ; then
        echo "ARM"
    elif [[ "$arch" == "riscv" ]] ; then
        echo "RISCV"
    else
        error_and_exit "Unknown architecture '$arch'"
    fi
}

# Get the arch name used by m5 (i.e, x86 instead of x86_64)
get_m5_arch_name() {
    local arch="$1"
    if [[ "$arch" == "x86_64" ]] ; then
        echo "x86"
    elif [[ "$arch" == "aarch64" ]] ; then
        echo "arm64"
    elif [[ "$arch" == "riscv" ]] ; then
        echo "riscv"
    else
        error_and_exit "Unknown architecture '$arch'"
    fi
}

# returns the path relative to $GEM5_ROOT to the GEM5 executable for a given architecture, protocol and build_type
get_gem5_binary() {
    local arch="$1"
    local protocol="$2"
    local build_type="$3"
    if [[ "$protocol" == "None" ]] ; then
        echo "build/$(get_gem5_arch_name ${arch})/gem5.${build_type}"
    else
        echo "build/$(get_gem5_arch_name ${arch})_${protocol}/gem5.${build_type}"
    fi
}


## Misc functions

declare -A ESCAPE_SEQUENCES
ESCAPE_SEQUENCES=(
    [red]=$'\e[31m'
    [green]=$'\e[32m'
    [yellow]=$'\e[33m'
    [blue]=$'\e[34m'
    [magenta]=$'\e[35m'
    [cyan]=$'\e[36m'
    [white]=$'\e[37m'
    [normal]=$'\e[0m'
)

color() {
    local color="$1"
    local text="$2"
    [[ $# -eq 2 ]] || { echo "Procedure 'color' needs exactly 2 arguments." ; exit 1; }
    printf "%s%s%s" "${ESCAPE_SEQUENCES[$color]}" "$text" "${ESCAPE_SEQUENCES[normal]}"
}

get_num_processors() {
    grep -c -e $'^processor\t\\+: ' /proc/cpuinfo
}

simpler_getopt() {
    local optspec="$1"
    shift
    local errors="$(getopt -n error-detected -Q --options "" --long ${optspec} -- "$@" 2>&1 || true)"
    if [[ "$errors" =~ error-detected:.+ ]] ; then
       error_and_exit "$(echo "$errors" | cut -d: -f2- | xargs -n1 -d'\n' -i% -- echo "Error:%.")"
    fi
    getopt -n tasks-gem5 --options "" --long ${optspec} -- "$@"
}

# Tests if the first argument is repeated 
list_contains() {
    local i="$1"
    shift
    for j in "$@" ; do
        echo "$j"
    done | grep -q -e "^$i\$"
}

# Prints all its arguments except those equal to the first
remove_from_list() {
    local i="$1"
    shift
    for j in "$@" ; do
        echo "$j"
    done | grep -v -e "^$i\$"
}
