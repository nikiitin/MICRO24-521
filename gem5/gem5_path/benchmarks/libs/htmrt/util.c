#define _POSIX_C_SOURCE 200112L // pthread_barrier

#include <stdint.h>
#include <unistd.h>

#include "env_globals.h"
#include "util.h"

bool parseBoolEnv(char *envVarName, bool *envVar) {
    // Sets envVar if "envVarName" defined and set to a valid int value

    char *envString = getenv(envVarName);
    if (envString != NULL) {
        char* end;
        long value = strtol(envString, &end, 10);

        if (!*end) {
            assert((value == 1) || (value == 0)); // boolean
            // Converted successfully
            *envVar = value;
            return true;
        }
        else {
            fprintf(stderr, "Invalid value for %s env var\n",
                    envVarName);
            exit(1);
        }
    }
    return false;
}

bool parseLongIntEnv(char *envVarName, long int *envVar) {
    // Sets envVar if "envVarName" defined and set to a valid int value

    char *envString = getenv(envVarName);
    if (envString != NULL) {
        char* end;
        long value = strtol(envString, &end, 10);

        if (!*end) {
            // Converted successfully
            assert(value > 0);
            *envVar = value;
            return true;
        }
        else {
            fprintf(stderr, "Ignoring invalid value for %s env var\n",
                    envVarName);
            exit(1);
        }
    }
    return false;
}

// This function is called from SimStartup (part of TM_STARTUP macro)
void setEnvGlobals(int numThreads) {
    assert(sizeof(_env_globals_t) == CACHE_LINE_SIZE_BYTES);

    assert((numThreads > 0) && (numThreads < 256)); // Sanity checks

    // Allocate mem and init barrier
    sh_globals.barrier =
        (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    int s = pthread_barrier_init(sh_globals.barrier, NULL, numThreads);
    assert(s == 0);

    // Set default values
    env.config.inSimulator = 0;
    env.config.numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    env.config.htm_max_retries = DEFAULT_HTM_MAX_RETRIES;

    assert(env.config.numCPUs >= 1);
    bool set = parseBoolEnv(ENV_VAR_IN_SIMULATOR,
                            &env.config.inSimulator);
    if (!set) {
        fprintf(stderr, "WARNING %s env var is unset!\n",
                ENV_VAR_IN_SIMULATOR);
    }
    long result = -1;
    set = parseLongIntEnv(ENV_VAR_HTM_MAX_RETRIES, &result);
    if (set) {
        assert(result >= 0 && result < UINT8_MAX);
        env.config.htm_max_retries = (uint8_t) result;
    }
    set = parseBoolEnv(ENV_VAR_HTM_HEAP_PREFAULT,
                            &env.config.heapPrefault);
    if (!set) {
        fprintf(stderr, "WARNING %s env var is unset!\n",
                ENV_VAR_HTM_HEAP_PREFAULT);
    }
    set = parseBoolEnv(ENV_VAR_HTM_BACKOFF,
                       &env.config.backoff);
    if (!set) {
        fprintf(stderr, "WARNING %s env var is unset!\n",
                ENV_VAR_HTM_BACKOFF);
    }
}
