#include "env.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
