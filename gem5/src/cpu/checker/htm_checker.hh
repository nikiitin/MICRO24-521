/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __CPU_HTM_CHECKER_HH__
#define __CPU_HTM_CHECKER_HH__

#include <iostream>
#include <utility>
#include <vector>

#include "base/trace.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "cpu/checker/base_htm_checker.hh"
#include "cpu/o3/comm.hh"
#include "sim/lockstep.hh"
#include "sim/value_record.hh"

namespace gem5
{

class HTMChecker : public BaseHTMChecker
{
public:
    class Recorder
    {
        friend class HTMChecker;
    public:
        /** Constructs a recorder.
         */
        Recorder(const std::string &_my_name,
                 BaseCPU *_cpu,
                 HTMChecker *_checker);

        /** Destructor. */
        ~Recorder() {}

        std::string name() const { return _name; };
        void createFifo();
        void openFifo();

    private:
        void commit(uint64_t xid);
        void abort();
        void begin(uint64_t xid);
        void recordValue(bool isStore, Trace::InstRecord *traceData);
        /** The object name, for DPRINTF.  We have to declare this
         *  explicitly because HTM is not a SimObject. */
        const std::string _name;
        /** Pointer to the CPU. */
        BaseCPU *cpu;
        /** Pointer to the parent checker object */
        HTMChecker *checker;
    };

    class Replayer
    {
        friend class HTMChecker;
    public:
        /** Constructs a Replayer.
         */
        Replayer(const std::string &_my_name,
                 BaseCPU *_cpu,
                 HTMChecker *checker);

        /** Destructor. */
        ~Replayer() {}
        std::string name() const { return _name; };
        void openFifo();
    private:
        bool canReplay(uint64_t xid);
        void begin(uint64_t xid);
        void commit(uint64_t xid);
        void checkValue(bool isStore, Trace::InstRecord *traceData);

        /** The object name, for DPRINTF.  We have to declare this
         *  explicitly because HTM is not a SimObject. */
        const std::string _name;

        /** Pointer to the CPU. */
        BaseCPU *cpu;
        /** Pointer to the parent checker object */
        HTMChecker *checker;
        /** Local index in the vector of values for the current transaction */
        int currentValueRecordLocalIndex;
        bool inSyncWithRecorder;
        /* Hacky way of dealing with the fact that the thread assigned
           to this cpu by the OS in the replayer may be different and
           thus its stack base address changes */
        bool fixedStackAddr;
        int stackAddrOffset;
        int retiredMemRefIgnoredBegin;
        int retiredMemRefIgnoredEnd;
    };


    HTMChecker(const std::string &_my_name,
               BaseCPU *_cpu);

    /** Destructor. */
    ~HTMChecker() {}

    void create() { createFifos(); };
    void open() { openFifos(); };
    bool canReplay(uint64_t xid);
    void begin(uint64_t xid);
    void commit(uint64_t xid);
    void abort();
    void retireInst(bool isMemRef, bool isTransactional,
                    Trace::InstRecord *traceData);
private:
    void openFifos();
    void createFifos();
    bool isLock(Trace::InstRecord *traceData);
    bool isUnlock(Trace::InstRecord *traceData);
    bool foundLocked(Trace::InstRecord *traceData);
    bool foundUnlocked(Trace::InstRecord *traceData);
    void getLockValue(Trace::InstRecord *traceData,
                      uint64_t &value);
    bool isReturnToUserMode(Trace::InstRecord *traceData);
    bool isLeavingUserMode(Trace::InstRecord *traceData);

    /** The object name, for DPRINTF.  We have to declare this
     *  explicitly because HTM is not a SimObject. */
    const std::string _name;
    /** Pointer to the CPU. */
    BaseCPU *cpu;
    Recorder recorder;
    Replayer replayer;
    bool hasFallbackLock;
    uint64_t lastFallbackLockReadValue;
    Addr fallbackLockAddr;
    /* File descriptor of the fifo used for communicating value
     * recoreds to the replay cpu */
    int valuesFifoFileDescriptor;
    /* Current seq no of the value record in the transaction */
    uint64_t valueRecordGlobalSeqNo;
    /* Load/store values recorded during the transaction */
    std::vector<ValueRecord> values;
    /* Address of the fallback lock */
    Addr fallbackLockVirtAddr;
    /* Last PC seen by checker */
    Addr lastNPC;
    std::string lastInstName;
    /* PC of the instruction that faulted during irrevocable
     * transaction execution, used to disable recording during faults
     * (i.e. not in user code) */
    Addr faultPC;
    // ARM specific stuff
    Addr prfmPC;
  enum ArmISALockStatus
  {
      NotAcquired,
      Acquiring,
      Acquired,
      Locked
  };
    ArmISALockStatus lockStatus = ArmISALockStatus::NotAcquired;

};

} // namespace gem5

#endif //  __CPU_HTM_CHECKER_HH__
