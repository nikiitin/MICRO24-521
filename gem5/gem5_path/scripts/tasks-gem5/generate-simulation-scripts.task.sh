
declare_task "generate-simulation-scripts" "Generate simulation scripts.
"

task_generate-simulation-scripts() {
    local gen_script_py="${GEM5_ROOT}/gem5_path/scripts/gen-scripts.py"
    echo "$(color "cyan" "This task is just a placeholder. It is equivalent to running '$gen_script_py'.")"
    echo "$(color "cyan" "Press Return to continue..")"
    read
    "$gen_script_py" "$@"
}

