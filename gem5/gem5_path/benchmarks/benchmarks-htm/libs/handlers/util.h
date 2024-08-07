#include <stdbool.h>

void setEnvGlobals(int numThreads);
void catProcMaps(const char* out_filename);
void dumpValueToHostFileSystem(long value, const char *out_filename);
void annotateCodeRegionBegin(uint64_t codeRegionId);
void annotateCodeRegionEnd(uint64_t codeRegionId);
void simWorkBegin();
void simWorkEnd();
void beginRegionOfInterest();
void endRegionOfInterest();
