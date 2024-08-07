/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __SIM_LOCKSTEP_HH__
#define __SIM_LOCKSTEP_HH__

#include <string>
#include <vector>

#include "enums/LockStepMode.hh"
#include "sim/value_record.hh"

namespace gem5
{

class System;

class Lockstep
{
public:
    Lockstep(System *_sys, enums::LockStepMode mode,
             std::string fifoRootPath);
    ~Lockstep();
    std::string getValuesFifoPath(int cpuId) const {
        return fifoRootPath + "/xactValues-cpu" +
            std::to_string(cpuId); }
    static enums::LockStepMode stringToLockstepMode(std::string mode)
    {
        if (mode == "record")
            return enums::record;
        else if (mode == "replay")
            return enums::replay;
        else {
            assert(mode == "disabled");
            return enums::disabled;
        }
    }

    enums::LockStepMode getMode() { return mode; }
    void setMode(enums::LockStepMode _mode) { mode = _mode; }

    /* Returns the file descriptor of the created FIFO*/
    int getFifoFileDescriptor() const;

    bool canReplay(int cpuId, uint64_t xid);
    void markReplayed(int cpuId, uint64_t xid);
    void recordCommit(int cpuId, uint64_t xid);

    void dumpRecordedValuesToFile(std::vector<ValueRecord> &values,
                                  int fileDescriptor);
    void readReplayValuesFromFile(std::vector<ValueRecord> &replayValues,
                                  int fileDescriptor, int nextSeqNum);
    void endRecording();
                     private:
    std::string getCommitOrderFifoPath() const {
        return fifoRootPath + "/commitOrder"; } // Hacky
    void dumpValueToFile(ValueRecord& record, int fileDescriptor);

    System *system;
    enums::LockStepMode  mode;
    std::string fifoRootPath;
    int commitOrderFifoFileDes;
};

} // namespace gem5

#endif
