/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __SIM_COMMIT_ORDER_HH__
#define __SIM_COMMIT_ORDER_HH__

#define _unused(x) ((void)(x))

#include <mutex>
#include <utility>

#include "sim/lockstep.hh"
#include "sim/system.hh"

namespace gem5
{

#define COMMIT_ORDER_RECORD_TYPE_WORKUNIT ('w')
#define COMMIT_ORDER_RECORD_TYPE_ATOMIC ('a')

class System;

class CommitOrder
{
public:
    CommitOrder(System *_sys, int _fd);
    ~CommitOrder();

    void record(char type, int tid, int tag);
    bool canReplay(char type, int tid, int tag);
    void recordEOF();
    void markReplayed(char type, int tid, int tag);
private:
    void record(char type, int tid, int tag,
                int fileDescriptor);
    void markReplayedPriv(char type, int tid, int tag);
    bool canReplayPriv(char type, int tid, int tag);
    bool canReplay(char type, int tid, int tag,
                   int fileDescriptor);
    void recordEOF(int fileDescriptor);
    bool isValid(char type, int tid, int tag) const;
    bool readFromFile(char &type, int &tid, int &tag,
                      int fileDescriptor);
    bool isEOF(char type, int tid, int tag);
    const std::string toString(char type, int tid, int tag) const;
    void fromString(char *buf, char &type, int &tid, int &tag) const;

    System* system;
    int fileDescriptor;
    char nextCommitToReplay_type;
    int nextCommitToReplay_tag;
    int nextCommitToReplay_tid;
    // Access to nextCommitToReplay needs synchronization when
    // replaying using KVM
    bool synchronized;
    std::mutex replayMutex;
};

} // namespace gem5

#endif //  __SIM_COMMIT_ORDER_HH__
