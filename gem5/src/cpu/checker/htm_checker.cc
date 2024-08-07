/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "cpu/checker/htm_checker.hh"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "debug/HTMChecker.hh"
#include "enums/LockStepMode.hh"
#include "mem/request.hh"
#include "sim/system.hh"

namespace gem5
{

#define _unused(x) ((void)(x))

using namespace std;

HTMChecker::HTMChecker(const std::string &_my_name,
                       BaseCPU *_cpu) :
    BaseHTMChecker(_my_name, _cpu),
    _name(_my_name),
    cpu(_cpu),
    recorder(_my_name + ".recorder", _cpu, this),
    replayer(_my_name + ".replayer", _cpu, this),
    hasFallbackLock(false),
    lastFallbackLockReadValue(0),
    valuesFifoFileDescriptor(-1),
    valueRecordGlobalSeqNo(0),
    fallbackLockVirtAddr(0),
    lastNPC(0),
    faultPC(0)
{
    if (cpu->system->getHTM() != NULL) {
        fallbackLockVirtAddr =
            cpu->system->getHTM()->getFallbackLockVAddr();
    } else {
        panic("HTM system configuration unset!\n");
    }
}


HTMChecker::Recorder::Recorder(const std::string &_my_name,
                               BaseCPU *_cpu,
                               HTMChecker *_checker) :
    _name(_my_name),
    cpu(_cpu),
    checker(_checker)
{

}

HTMChecker::Replayer::Replayer(const std::string &_my_name,
                               BaseCPU *_cpu,
                               HTMChecker *_checker) :
    _name(_my_name),
    cpu(_cpu),
    checker(_checker),
    currentValueRecordLocalIndex(0),
    inSyncWithRecorder(false),
    retiredMemRefIgnoredBegin(0),
    retiredMemRefIgnoredEnd(0)
{
}

void
HTMChecker::begin(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::record) {
        recorder.begin(xid);
    } else if (cpu->system->getLockstepMode() == enums::replay) {
        // replayer begins transactions that immediately abort, so
        // ignore this.. value checking begins when lock acquisition
        // detected via retiredMemRef
    } else {
    }
}

void
HTMChecker::commit(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::record) {
        // called upon htm_stop instruction
        recorder.commit(xid);
    }
    else if (cpu->system->getLockstepMode() == enums::replay) {
        // replayer can never commit a transaction, values are
        // checked when lock released detected by retiredMemRef

        // NOTE: This can happen if forceHtmDisabled does not prevent
        // transaction start
        panic("Unexpected transaction commit in replayer!\n");
    } else {
        assert(cpu->system->getLockstepMode() == enums::disabled);
    }
}


void
HTMChecker::Recorder::begin(uint64_t xid)
{
    // Called when lock acquisition detected
    assert(checker->values.empty());
}

bool
HTMChecker::canReplay(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::replay) {
        return replayer.canReplay(xid);
    } else {
        return true;
    }
}

bool
HTMChecker::Replayer::canReplay(uint64_t xid) {
    if (!cpu->system->getLockstepManager()->
        canReplay(cpu->cpuId(), xid)) {
        // Cannot begin transaction, not next in commit order
        return false;
    }

    DPRINTF(HTMChecker, "replayer allowed replay\n");
    return true;
}

void
HTMChecker::Replayer::begin(uint64_t xid) {
    assert(checker->values.empty());
    assert(checker->hasFallbackLock);
    // Read recorded values from pipe
    cpu->system->getLockstepManager()->
        readReplayValuesFromFile(checker->values,
                                 checker->valuesFifoFileDescriptor,
                                 checker->valueRecordGlobalSeqNo);
    DPRINTF(HTMChecker, "replayer begin\n");
}

void
HTMChecker::Recorder::commit(uint64_t xid)
{
    DPRINTF(HTMChecker, "recorder commit\n");
    // Record this commit in global pipe to release transaction
    // running in lockstep (replayer)
    cpu->system->getLockstepManager()->
        recordCommit(cpu->cpuId(), xid);

    cpu->system->getLockstepManager()->
        dumpRecordedValuesToFile(checker->values,
                                 checker->valuesFifoFileDescriptor);

    checker->values.clear();
}

void
HTMChecker::Replayer::commit(uint64_t xid) {
    assert(checker->hasFallbackLock);
    // Called upon detection of fallback lock release
    cpu->system->getLockstepManager()->
        markReplayed(cpu->cpuId(), xid);

    DPRINTF(HTMChecker, "replayer commit\n");
    // Must have checked all records before commit
    if (currentValueRecordLocalIndex != checker->values.size()) {
        panic("Value check failed: commit reached"
              " before all values were checked\n");
    }
    currentValueRecordLocalIndex = 0; // Reset index
    inSyncWithRecorder = false;
    assert(retiredMemRefIgnoredBegin < 10); // TODO: Automate check
    assert(retiredMemRefIgnoredEnd < 2); // TODO: Automate check
    retiredMemRefIgnoredBegin = 0;
    retiredMemRefIgnoredEnd = 0;
    checker->values.clear();
}

void
HTMChecker::abort() {
    if (cpu->system->getLockstepMode() == enums::record) {
        // Clear all recorded values upon abort
        recorder.abort();
    } else if (cpu->system->getLockstepMode() == enums::replay) {
        // Replayer ignores abort signals while spinning on xbegin
        // before canReplay returns true
    }
}

void
HTMChecker::Recorder::abort()
{
    checker->valueRecordGlobalSeqNo -= checker->values.size();
    checker->values.clear();
    DPRINTF(HTMChecker, "recorder abort\n");
}

bool
HTMChecker::isLock(Trace::InstRecord *traceData)
{
    if (cpu->system->getArch() == Arch::X86ISA) {
        bool isStore = traceData->getStaticInst()->isStore();
        bool isPrefetch = traceData->getStaticInst()->isPrefetch();
        if (!isStore || isPrefetch) return false;
        return ((traceData->getIntData() == 1) &&
                (lastFallbackLockReadValue == 0));
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        assert(traceData->getMemValid());
        if (lockStatus == ArmISALockStatus::Acquired) {
            lockStatus = ArmISALockStatus::Locked;
            return true;
        } else {
            return false;
        }
    } else {
        panic("Lockstep: lock interception not tested in this ISA!");
        return false;
    }
}

bool
HTMChecker::isUnlock(Trace::InstRecord *traceData)
{
    if (cpu->system->getArch() == Arch::X86ISA) {
        bool isStore = traceData->getStaticInst()->isStore();
        bool isPrefetch = traceData->getStaticInst()->isPrefetch();
        if (!isStore || isPrefetch) return false;
        if (traceData->getIntData() == 0) {
            assert(hasFallbackLock &&
                   (lastFallbackLockReadValue == 1));
            return true;
        } else {
            return false;
        }
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        if (traceData->getStaticInst()->getName() == "stlrh") {
            // unlock done via stlrh (store release)
            assert(lockStatus == ArmISALockStatus::Locked);
            lockStatus = ArmISALockStatus::NotAcquired;
            return true;
        } else {
            return false;
        }
    } else {
        panic("Lockstep: unlock interception not tested in this ISA!");
        return false;
    }
}

bool
HTMChecker::foundLocked(Trace::InstRecord *traceData)
{
    if (cpu->system->getArch() == Arch::X86ISA) {
        return (traceData->getIntData() == 1);
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        // Simply ignore value
        return true;
    } else {
        panic("Lockstep: lock interception not tested in this ISA!");
        return false;
   }
}

bool
HTMChecker::foundUnlocked(Trace::InstRecord *traceData)
{
    if (cpu->system->getArch() == Arch::X86ISA) {
        return (traceData->getIntData() == 0);
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        // Simply ignore value
        return true;
    } else {
        panic("Lockstep: lock interception not tested in this ISA!");
        return false;
   }
}

void
HTMChecker::getLockValue(Trace::InstRecord *traceData,
                         uint64_t &value)
{
    if (cpu->system->getArch() == Arch::X86ISA) {
        bool isStore = traceData->getStaticInst()->isStore();
        bool isPrefetch = traceData->getStaticInst()->isPrefetch();
        if (!isStore && !isPrefetch) {
            value = traceData->getIntData();
        }
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        /* This hack to detect lock/unlock without having to look into
           the lock value is dependant on the particular lock
           implementation. See the ticket lock implemenation in :
           gem5_path/benchmarks/benchmarks-htm/libs/handlers/spinlock.h
           It is assumed that the instruction at the "unlock" tag sits
           48 bytes (12 instructions) after the first instruction that
           access the lock:
        */
        if (traceData->getStaticInst()->getName() == "prfm") {
            // STEP 1: seen first instruction, record PC and set
            // acquiring to start monitoring PC to detect successful
            // acquisition via getLockValue
            assert(lockStatus == ArmISALockStatus::NotAcquired);
            prfmPC = traceData->getPCState().instAddr();
            lockStatus = ArmISALockStatus::Acquiring;
        } else if ((lockStatus == ArmISALockStatus::Acquiring) &&
            traceData->getPCState().instAddr() == prfmPC+48) {
            // STEP 2: Signal lock acquisition via lockStatus, will be
            // eventually observed by isLock()
            lockStatus = ArmISALockStatus::Acquired;
        }
        value = traceData->getIntData();
    } else {
        panic("Lockstep: lock interception not tested in this ISA!");
    }
}

bool
HTMChecker::isLeavingUserMode(Trace::InstRecord *traceData)
{
    assert(faultPC == 0);
    if (cpu->system->getArch() == Arch::X86ISA) {
        if (traceData->getPCState().microPC() >= 32768) {
            faultPC = traceData->getPCState().instAddr();
            return true;
        }
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        if (traceData->getPCState().instAddr() & 0xffffff0000000000) {
            faultPC = lastNPC;
            // Current PC is already kernel PC, need to
            // save preceding usermode PC
            return true;
        }
    } else {
        panic("Lockstep: leaving user mode not tested in this ISA!");
    }
    return false;
}

bool
HTMChecker::isReturnToUserMode(Trace::InstRecord *traceData)
{
    assert(faultPC != 0);
    if (cpu->system->getArch() == Arch::X86ISA) {
        if (faultPC == traceData->getPCState().instAddr()) {
            if (traceData->getPCState().microPC() < 32768) {
                return true;
            } else {
                DPRINTF(HTMChecker, "Current PC matches fault PC %#x"
                        " but microPC is not 0 (upc=%#x)\n",
                        faultPC, traceData->getPCState().microPC());
            }
        }
    } else if (cpu->system->getArch() == Arch::ArmISA) {
        if (faultPC == traceData->getPCState().instAddr()) {
            if (lastInstName == "eret") {
                return true;
            } else {
                panic("Return to user mode expects eret!");

            }
        }
    } else {
        panic("Lockstep: return to user mode not tested in this ISA!");
    }
    return false;
}

void
HTMChecker::retireInst(bool isMemRef, bool isTransactional,
                       Trace::InstRecord *traceData) {
    if (cpu->system->getLockstepMode() == enums::record ||
        cpu->system->getLockstepMode() == enums::replay) {
        if (isTransactional || hasFallbackLock) {
            // Detect entry/exit into/from kernel during
            // transactions (no record/replay)
            if (faultPC != 0){
                if (isReturnToUserMode(traceData)) {
                    faultPC = 0;
                    DPRINTF(HTMChecker, "Resuming value recording after "
                            "handling interrupt/fault at PC %#x\n",
                            faultPC);
                }
            } else if (faultPC == 0) {
                // Detect trap to kernel code
                if (isLeavingUserMode(traceData)) {
                    // Save int/fault pc
                    DPRINTF(HTMChecker, "Skipping value recording while "
                            "handling interrupt/fault at PC %#x\n",
                            faultPC);
#if 0 // Uncomment this code to debug segmentation faults: simulate
      // commit on the recorder to allow the replayer to check the
      // values up to the point where the segfault occurs
                    recorder.commit(0);
                    DPRINTF(HTMChecker, "Dumping recorder's committed values"
                            " before segmentation fault at PC %#x\n",
                            faultPC);
#endif
                }
            }
        }
    }
    assert(traceData->getStaticInst()->isMemRef() == isMemRef);

    if (isMemRef) {
        if (traceData->getStaticInst()->isHtmCmd()) {
            // Skip htm commands
        } else if (((traceData->getAddr() == fallbackLockVirtAddr) &&
                     traceData->getStaticInst()->isStore()) ||
                   (lockStatus == ArmISALockStatus::Acquiring)) {
            getLockValue(traceData, lastFallbackLockReadValue);
            if (isUnlock(traceData)) { // Unlock
                DPRINTF(HTMChecker, "lock released\n");
                // Check replayed values at end of critical section
                if (cpu->system->getLockstepMode() == enums::replay) {
                    // Notify replayer
                    replayer.commit(0);
                } else if (cpu->system->
                           getLockstepMode() == enums::record) {
                    // Record values of non-spec transaction
                    recorder.commit(0);
                }
                assert(hasFallbackLock);
                hasFallbackLock = false;
            } else if (isLock(traceData)) { // Lock
                DPRINTF(HTMChecker, "lock acquired\n");
                assert(!hasFallbackLock);
                hasFallbackLock = true;
                if (cpu->system->getLockstepMode() == enums::replay) {
                    replayer.begin(0);
                } else if (cpu->system->
                           getLockstepMode() == enums::record) {
                    // Record values of non-spec transaction
                    recorder.begin(0);
                }
            } else if (foundLocked(traceData)) {
                /* Stored value may be 1 if read data was 1
                   (lock already acquired), so need to check last
                   value seen for lock in order to detect if this is a
                   successful "acquire" */
                DPRINTF(HTMChecker, "Store found busy lock\n");
            } else if (foundUnlocked(traceData)) {
                DPRINTF(HTMChecker, "%s found free lock\n",
                        traceData->getStaticInst()->getName());
            } else {
                panic("Unexpected value for fallback lock");
            }
        } else if (traceData->getAddr() == fallbackLockVirtAddr) {
            // Load access to the lock: do not record/check
            getLockValue(traceData, lastFallbackLockReadValue);
            if (cpu->system->getLockstepMode() == enums::record) {
                if (isTransactional) {
                    assert(foundUnlocked(traceData) ||
                           // Lock subscription immediately after
                           // xbegin found lock held
                           values.empty());
                } else {
                    if (!values.empty()) { // recording with the lock
                        assert(hasFallbackLock);
                        assert(foundLocked(traceData));
                    } else { // Not yet recording: maybe an access
                             // during the abort handler
                        assert(!hasFallbackLock);
                    }
                }
            } else if (cpu->system->getLockstepMode() == enums::replay) {
                assert(!isTransactional);
                if (foundLocked(traceData)) {
                    if (!values.empty()) {
                        // Load prior to unlocking during commitTransaction
                        assert(hasFallbackLock);
                    } else {
                        // Some other thread acquired the lock, this
                        // threads spins in the abort handler
                        assert(!hasFallbackLock);
                    }
                } else if (foundUnlocked(traceData)) {
                    if (values.empty()) {
                        // Load prior to locking before beginTransaction
                        assert(!hasFallbackLock);
                    } else {
                        // Should never find lock free during value replay
                        panic("Unexpected value for fallback lock");
                    }
                } else {
                    panic("Found fallback lock neither busy nor free?");
                }
            }
        } else { // Access to a regular addr
            bool isStore = traceData->getStaticInst()->isStore();
            if (cpu->system->getLockstepMode() == enums::record) {
                if (isTransactional || hasFallbackLock) {
                    recorder.recordValue(isStore, traceData);
                }
            }
            else if (cpu->system->
                     getLockstepMode() == enums::replay) {
                if (hasFallbackLock) {
                    replayer.checkValue(isStore, traceData);
                }
            }
        }
    }
    lastNPC = traceData->getPCState().nextInstAddr();
    lastInstName = traceData->getStaticInst()->getName();
}

void
HTMChecker::openFifos()
{
        // HTM Checker support
    if (cpu->system->getLockstepMode() == enums::replay) {
        replayer.openFifo();
    }
    else if (cpu->system->getLockstepMode() == enums::record) {
        recorder.openFifo();
    }
}

void
HTMChecker::createFifos()
{
    if (cpu->system->getLockstepMode() == enums::record) {
        recorder.createFifo();
    }
}

void
HTMChecker::Recorder::createFifo()
{
    if (cpu->system->getLockstepMode() == enums::record) {

        std::string fifoPath = cpu->system->getLockstepManager()->
            getValuesFifoPath(cpu->cpuId());

        /* create and open the FIFO (named pipe) */
        if (mkfifo(fifoPath.c_str(), 0666) < 0) {
            perror("cannot make xact values fifo");
            fatal("Lockstep mode (record): "
                  "failed to create named pipe");
        }
    }
}

void
HTMChecker::Recorder::openFifo()
{
    assert(cpu->system->getLockstepMode() == enums::record);
    //  Opening the read or write end of a FIFO blocks until the
    //  other end is also opened
    std::string fifoPath = cpu->system->getLockstepManager()->
        getValuesFifoPath(cpu->cpuId());
    int fd = ::open(fifoPath.c_str(), O_WRONLY);
    if (fd < 0) {
        perror("cannot open xact values fifo");
        fatal("Lockstep mode (record): "
              "failed to open named pipe");
    }
    else {
        assert(fd >= 0);
        checker->valuesFifoFileDescriptor = fd;
    }
}


void
HTMChecker::Replayer::openFifo() {

    if (cpu->system->getLockstepMode() == enums::replay) {
        std::string fifoPath = cpu->system->getLockstepManager()->
            getValuesFifoPath(cpu->cpuId());
        int fd;
        do {
            fd = ::open(fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
        }
        while (fd < 0);
        checker->valuesFifoFileDescriptor = fd;
    }
}


void
HTMChecker::Recorder::recordValue(bool isStore, Trace::InstRecord *traceData) {
    if (!traceData)
        fatal("lockStep mode requires 'Exec' debug flag set\n");;

    uint64_t addr = traceData->getAddr();
    uint64_t value = traceData->getIntData();
    int data_status = traceData->getDataStatus();
    if ((data_status == Trace::InstRecord::DataVec) ||
        (data_status == Trace::InstRecord::DataVecPred)) {
        //TheISA::VecRegContainer& vec_value = getVecData();

        // TODO: Extend ValueRecord to pass VecData values
        // For now, simply pass a dummy value and have the replayer
        // skip the value check for this record.
        value = 0xCAFEBABEDEADC0DE;
    }

    Addr pc = traceData->getPCState().instAddr();
    MicroPC upc = traceData->getPCState().microPC();

    if (checker->hasFallbackLock) {
        if (checker->faultPC != 0) {
            DPRINTF(HTMChecker, "skipping value record during faults\n");
            // Do not record mem refs from interrupts/faults
            return;
        }
    }
    DPRINTF(HTMChecker,
            "value recorded for PC: %#x.%d (global seqno: %d)\n",
            pc, upc, checker->valueRecordGlobalSeqNo);

    // Records each value loaded and stored by the transaction
    checker->values.push_back(ValueRecord(isStore, pc, upc, addr, value,
                                 checker->valueRecordGlobalSeqNo++));
}

void
HTMChecker::Replayer::checkValue(bool isStore, Trace::InstRecord *traceData) {
    if (!traceData)
        fatal("lockStep mode requires 'Exec' debug flag set\n");;

    if (checker->faultPC != 0) {
        // Do not check mem refs from interrupts/faults
        DPRINTF(HTMChecker, "skipping value replay during faults\n");
        return;
    }

    uint64_t value = traceData->getIntData();
    int data_status = traceData->getDataStatus();
    bool vec_value = false;
    if ((data_status == Trace::InstRecord::DataVec) ||
        (data_status == Trace::InstRecord::DataVecPred)) {
        //TheISA::VecRegContainer& vec_value = getVecData();
        vec_value = true;
    }

    uint64_t address = traceData->getAddr();
    Addr pc = traceData->getPCState().instAddr();
    MicroPC upc = traceData->getPCState().microPC();

    if (!inSyncWithRecorder) {
        assert(currentValueRecordLocalIndex == 0);
        // Search value record for matching return (matching
        // target address in the stack and return address)
        for (int i=0; i < checker->values.size(); ++i) {
            ValueRecord& record = checker->values[i];
            if (pc == record.pc &&
                upc == record.upc &&
                isStore == record.wasStore &&
                address == record.address &&
                value == record.value) {
                inSyncWithRecorder = true;
                currentValueRecordLocalIndex = i+1;
                checker->valueRecordGlobalSeqNo +=
                    currentValueRecordLocalIndex;
                DPRINTF(HTMChecker,
                        "replayer correctly sync'ed with value record\n");
                return;
            }
        }
        retiredMemRefIgnoredBegin++;
        return; // Wait until replayer in sync with recorder
    }
    if (currentValueRecordLocalIndex >= checker->values.size()) {
        // The recorded transaction must be non-speculative
        retiredMemRefIgnoredEnd++;
        return;
    }
    ValueRecord& record = checker->values[currentValueRecordLocalIndex];

    bool pcMismatch = false;
    bool typeMismatch = false;
    bool addrMismatch = false;
    bool valueMismatch = false;
    if (pc != record.pc) {
        pcMismatch = true;
    } else {
        assert(upc == record.upc);
    }
    if (isStore != record.wasStore) {
        typeMismatch = true;
    }
    if (address != record.address) {
        addrMismatch = true;
    }
    if (value != record.value) {
        if (vec_value) {
            assert(record.value == 0xCAFEBABEDEADC0DE);
            DPRINTF(HTMChecker,
                    "replayer ignores vec value record"
                    " for PC: %#x.%d (global seqno: %d)\n",
                    pc, upc, checker->valueRecordGlobalSeqNo);
        } else {
            valueMismatch = true;
        }
    }

    if (pcMismatch || typeMismatch || addrMismatch || valueMismatch) {
        if (!pcMismatch && !typeMismatch && !valueMismatch) {
            // Only address mismatch, this could be due to running a
            // different thread than the recorder cpu
            if ((address & 0xffffff0000000000) == 0x7f0000000000) {
                // mistmatched address in stack access?
                cerr << "It looks like CPU " << cpu->cpuId()
                     << " may be executing different threads in recorder"
                     << " and replayer simulations" << endl;
                cerr << "IMPORTANT NOTICE: Lockstep record/replay"
                     << " requires thread pinning (e.g. via set_affinity)"
                     << endl;
            }
        }
        cerr << "Replayer CPU " << setw(2) << cpu->cpuId()
             << " detected mismatch with recorder CPU ("
             << (valueMismatch ? "value" : (addrMismatch ? "address" : "type"))
             << ") Record/replay pairs:" << endl
             << "PC: " << hex << record.pc << "."
             << record.upc << "/" << pc << "." << upc << endl
             << "Address: " << hex << record.address << "/" << address << endl
             << "Value:   " << hex << record.value << "/" << value << endl
             << "Type: " << (record.wasStore ? "store" : "load")
             << "/" << (isStore ? "store" : "load") << endl
             << "Replayer CPU " << (checker->hasFallbackLock ?
                                    "has lock" : "in tx") << endl << dec
             << "GlobalSeqNo=" << checker->valueRecordGlobalSeqNo
             << " LocalIndex=" << currentValueRecordLocalIndex
             << endl;


        fatal("lockstep replayer detected mistmach with recorder CPU");
    }
    DPRINTF(HTMChecker,
            "value check passed for PC: %#x.%d (global seqno: %d)\n",
            pc, upc, checker->valueRecordGlobalSeqNo);


    _unused(value);
    _unused(address);
    _unused(record);

    checker->valueRecordGlobalSeqNo++;

    currentValueRecordLocalIndex++; // Move on to next record
}

} // namespace gem5
