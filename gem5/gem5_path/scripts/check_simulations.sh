#!/bin/bash

SCRIPT_DIR="$(readlink -fm "$(dirname "$0")")"
SCRIPT_COMMAND="$0"
set -o nounset
set -o pipefail
set -o errexit
trap 'echo "$SCRIPT_COMMAND: error $? at line $LINENO"' ERR

if [ $# -eq 1 ] ; then
    RESULTS_DIR="$1"
else
    echo "Usage: ${SCRIPT_COMMAND} results_dir" 1>&2
    exit 1
fi

[[ -d "$RESULTS_DIR" ]] || { echo "«${RESULTS_DIR}» is not a directory" 1>&2 ; exit 1 ; }

find "${RESULTS_DIR}" -name simulate -o -name simulate.sh | sort | while IFS="" read -r SIM_SCRIPT ; do
    SIM_DIR="$(dirname "${SIM_SCRIPT}")"
    SIM_CKPTDIR="${SIM_DIR}/ckpt"
    SIM_CKPTSIMOUT="${SIM_CKPTDIR}/simout"
    SIM_SIMOUT="${SIM_DIR}/simout"
    SIM_SIMERR="${SIM_DIR}/simerr"
    SIM_STDOUT="${SIM_DIR}/stdout"
    SIM_STDERR="${SIM_DIR}/stderr"
    #SIM_SCRIPT="${SIM_DIR}/simulate"
    SIM_STATS="${SIM_DIR}/stats.txt"

    if [[ ! -s "${SIM_STDOUT}" ]] ; then
        SIM_STATUS="NOSTDOUT"
    elif ! grep -q "#### Simulating from init checkpoint" "${SIM_STDOUT}" ; then
        if ! grep -q "#### Creating init checkpoint using" "${SIM_STDOUT}"  ; then
            SIM_STATUS="FAILED_CKPT"
            # Error
        elif grep -iq "Assertion" "${SIM_STDOUT}" ; then
            SIM_STATUS="FAILED_CKPT_Ben"
            # Error
        elif grep -iq "Segmentation" "${SIM_STDOUT}" ; then
            SIM_STATUS="FAILED_CKPT_Ben"
            # Error
        elif grep -qiE 'Binary.+cannot be found' "${SIM_STDOUT}" ; then
            SIM_STATUS="FAILED_CKPT_Ben"
            # Error
        elif grep -iq "#### Simulated terminal End   ####" "${SIM_STDOUT}" ; then
            SIM_STATUS="FAILED_CKPT_End"
            # Error
        else
            SIM_STATUS="NOCKPT"
            # Waiting
        fi
    elif grep -q "#### Failed to create init checkpoint " "${SIM_STDOUT}" ; then
        SIM_STATUS="FAILED_CKPT"
    elif [[ ! -f "${SIM_SIMOUT}" ]] ; then
        SIM_STATUS="NOSIMOUT"
    elif [[ ! -s "${SIM_SIMOUT}" ]] ; then
        SIM_STATUS="EMPTY_SIMOUT"
    elif grep -q "#### Simulation completed (m5 exit)" "${SIM_STDOUT}" ; then
        if grep -q "ARCH_NAME=x86_64" "${SIM_SCRIPT}" ; then
            SIM_TERMINAL="${SIM_DIR}/system.pc.com_1.device"
        elif grep -q "ARCH_NAME=aarch64" "${SIM_SCRIPT}" ; then
            SIM_TERMINAL="${SIM_DIR}/system.terminal"
        elif grep -q -e "arch='\?x86_64'\?" "${SIM_SCRIPT}" ; then
            SIM_TERMINAL="${SIM_DIR}/system.pc.com_1.device"
        elif grep -q -e "arch='\?aarch64'\?" "${SIM_SCRIPT}" ; then
            SIM_TERMINAL="${SIM_DIR}/system.terminal"
        elif grep -q -e "arch='\?riscv'\?" "${SIM_SCRIPT}" ; then
            echo "TODO"
            SIM_TERMINAL="${SIM_DIR}/???" # TODO
            exit 1
        else
            echo "Cannot determine architecture in «$SIM_SCRIPT»"
            exit 1
        fi
        if grep -iq "Assertion" "${SIM_TERMINAL}" ; then
            SIM_STATUS="FAILED_Bench"
        elif grep -iq "Segmentation" "${SIM_TERMINAL}" ; then
            SIM_STATUS="FAILED_Bench"
        else
            SIM_STATUS="COMPLETED_Full"
        fi
    elif grep -q "#### Simulation completed (end of ROI)" "${SIM_STDOUT}" ; then
        SIM_STATUS="COMPLETED_RoI"
    elif grep -q "#### Simulation failed" "${SIM_STDOUT}" ; then
        SIM_STATUS="FAILED"
    else
        FIRSTLINE="$(grep -v -E '^\gem5 Simulator System' "$SIM_SIMOUT")"
        if [ -n "${FIRSTLINE}" ] ; then
            # probably running now
            if grep "Assertion" "${SIM_SIMERR}" | grep -q "failed." ; then
                SIM_STATUS="FAILED_Assert"
            elif grep -q "^\panic:" "${SIM_SIMERR}" ; then
                if grep -q "assert failure" "${SIM_SIMERR}" ; then
                    SIM_STATUS="FAILED_Assert"
                else
                    SIM_STATUS="FAILED_Panic"
                fi
            elif grep -q "^Program aborted" "${SIM_SIMERR}" ; then
                SIM_STATUS="FAILED_Abort"
            elif grep -q "gem5 has encountered a segmentation fault!" "${SIM_SIMERR}" ; then
                SIM_STATUS="FAILED_Segv"
            elif grep -q "slurm_script: error" "${SIM_STDOUT}" ; then
                SIM_STATUS="FAILED"
            else
                LASTLINE="$(grep -v -E '^\.+$' "$SIM_SIMOUT" | tail -n1)" # remove lines consisting only of dots
                if [[ "$LASTLINE" =~ \.+[0-9]+ ]] ; then
                    SIM_STATUS="$(printf "R%14d" "$(echo $LASTLINE | tr -d .)")"
                else
                    SIM_STATUS="R    ??????????" # probably running, but it has not yet printed any interval number
                fi
            fi
        else
            SIM_STATUS="R    ??????????" # probably running, but it has not yet printed any interval number
        fi
    fi

    printf "%-15s\t%s\n" "${SIM_STATUS}" "${SIM_DIR}"
done
