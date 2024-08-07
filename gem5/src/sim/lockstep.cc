
#include "sim/lockstep.hh"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "base/types.hh"
#include "debug/Lockstep.hh"
#include "sim/commit_order.hh"
#include "sim/lockstep_macros.h" // Constants: REPLAY_RWSET_MARK_
#include "sim/sim_events.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

namespace gem5
{

Lockstep::Lockstep(System *_sys, enums::LockStepMode  _mode,
                   std::string _fifoRootPath) :
    system(_sys),
    mode(_mode),
    fifoRootPath(_fifoRootPath),
    commitOrderFifoFileDes(-1)
{
    std::string fifoPath = getCommitOrderFifoPath();
    if (mode == enums::record) {
        /* create fifo and open for writing if in record mode */
        if (mkfifo(fifoPath.c_str(), 0666) < 0) {
            perror("Lockstep mode (record): failed to create named pipe");
            mode = enums::disabled;
        }
        else {
            commitOrderFifoFileDes = open(fifoPath.c_str(), O_WRONLY);
            if (commitOrderFifoFileDes < 0) {
                perror("Lockstep mode (record): failed to open named pipe");
                mode = enums::disabled;
            }
        }
    }
    else if (mode == enums::replay) {
        /* Open fifo for reading if this system is in replay mode */
        do {
            commitOrderFifoFileDes =
                open(fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
        } while (commitOrderFifoFileDes < 0);
    }
    else {
        assert(mode == enums::disabled);
    }
}

int
Lockstep::getFifoFileDescriptor() const
{
    assert(commitOrderFifoFileDes >= 0);
    return commitOrderFifoFileDes;
}

bool
Lockstep::canReplay(int cpuId, uint64_t xid)
{
    return system->getCommitOrderManager()->
        canReplay(COMMIT_ORDER_RECORD_TYPE_ATOMIC,
                  cpuId, xid);
}

void
Lockstep::markReplayed(int cpuId, uint64_t xid)
{
    system->getCommitOrderManager()->
        markReplayed(COMMIT_ORDER_RECORD_TYPE_ATOMIC,
                     cpuId, xid);
}

void
Lockstep::recordCommit(int cpuId, uint64_t xid)
{
    system->getCommitOrderManager()->
        record(COMMIT_ORDER_RECORD_TYPE_ATOMIC,
               cpuId, xid);
}


void
Lockstep::endRecording()
{
    if (mode == enums::record) {
        system->getCommitOrderManager()->recordEOF();
    }
}

void
Lockstep::dumpValueToFile(ValueRecord& record, int fileDescriptor)
{
    char c;
    if (record.wasStore)
        c = REPLAY_RWSET_MARK_WRITE;
    else
        c = REPLAY_RWSET_MARK_READ;

    assert(fileDescriptor != -1);
    uint64_t seqNum = record.seqNum;

    Addr addr = record.address;
    uint64_t value = record.value;
    uint64_t pc = record.pc;
    uint64_t upc = record.upc;
    assert(upc < 0x100);
    std::stringstream sstream;
    sstream << c
            << std::setfill ('0') << std::hex
            << std::setw(REPLAY_RWSET_SEQNO_LEN)
            << seqNum
            << std::setw(REPLAY_RWSET_PC_LEN)
            << pc
            << std::setw(REPLAY_RWSET_UPC_LEN)
            << upc
            << std::setw(REPLAY_RWSET_ADDR_LEN)
            << addr
            << std::setw(REPLAY_RWSET_VALUE_LEN)
            << value ;
    std::string str = sstream.str();
    const char *buf = str.c_str();
    int bytes = ::write(fileDescriptor,
                        (void *)buf, str.length());
    assert(bytes == str.length());
    _unused(bytes);
    DPRINTF(Lockstep, "Dumping transactional record "
            "%ld to pipe: inst: %s "
            "addr: %#x value: %#x\n",
            seqNum,
            record.wasStore ? "store" : " load", addr, value);
}

void
Lockstep::dumpRecordedValuesToFile(std::vector<ValueRecord> &values,
                                  int fileDescriptor)
{
    char c = REPLAY_RWSET_MARK_BEGIN;
    // we want to call write in the global namespace, not this class
    int bytes = ::write(fileDescriptor, &c, 1);
    assert(fileDescriptor != -1);
    assert(bytes == 1);
    _unused(bytes);
    // Dump recorded read/written values to pipe
    auto value_it = values.begin();
    while (value_it != values.end()) {
        dumpValueToFile(*value_it, fileDescriptor);
        value_it++;
    }
    // Finally, mark end of RW sets
    c = REPLAY_RWSET_MARK_END;
    bytes = ::write(fileDescriptor, &c, 1);
    assert(bytes == 1);
}

void
Lockstep::readReplayValuesFromFile(std::vector<ValueRecord> &replayValues,
                                   int fileDescriptor, int nextSeqNum)
{
    assert(replayValues.empty());
    assert(fileDescriptor != -1);
    uint64_t valueRecordSeqNo = nextSeqNum;
    char c;
    /* Value records are dumped to pipe *AFTER* global commit record,
     * so replayGlobalCommitOrder can grant this tx permission to
     * begin before its value records are written to the pipe
     */
    int bytes;
    do {
        bytes = read(fileDescriptor, &c, 1);
    } while (bytes < 1);
    // read returns -1 when it would have blocked but nonblocking
    // operation was requested (fifo opened with O_NONBLOCK);

    // O3CPU granted transaction begin
    assert(c == REPLAY_RWSET_MARK_BEGIN);

    while (true) {
        /* TODO: Read incoming RW sets data from pipe. Format:
         * ({r|w}<addr><value>)*
         * r means the first value read on that memory location by the
         * transaction and w the last value written by the transaction
         * on that location. RW set data info ends when the character
         * 'c' (commit) is found.
         */
        do {
            bytes = read(fileDescriptor, &c, 1);
        } while (bytes <= 0);
        if (c == REPLAY_RWSET_MARK_END) {
            // End of the RW set dump: begin transaction
            break;
        }
        else {
            assert(c == REPLAY_RWSET_MARK_READ ||
                   c == REPLAY_RWSET_MARK_WRITE);
            bool wasStore = (c == REPLAY_RWSET_MARK_WRITE);
            // TODO: read the (address, value) pair (fixed length, hex)
            char seqnoBuf[REPLAY_RWSET_SEQNO_LEN+1]; // +1 for the \0
            bytes = 0;
            while (bytes < REPLAY_RWSET_SEQNO_LEN) {
                int read_bytes = read(fileDescriptor, &seqnoBuf[bytes],
                                      REPLAY_RWSET_SEQNO_LEN-bytes);
                if (read_bytes > 0) bytes += read_bytes;
            }
            assert(bytes == REPLAY_RWSET_SEQNO_LEN);
            seqnoBuf[bytes] = '\0';
            uint64_t seqNum = strtoul (seqnoBuf, NULL, 16);
            assert(seqNum == valueRecordSeqNo);
            valueRecordSeqNo++;

            char pcBuf[REPLAY_RWSET_PC_LEN+1]; // +1 for the \0
            bytes = 0;
            while (bytes < REPLAY_RWSET_PC_LEN) {
                int read_bytes = read(fileDescriptor, &pcBuf[bytes],
                                      REPLAY_RWSET_PC_LEN-bytes);
                if (read_bytes > 0) bytes += read_bytes;
            }
            assert(bytes == REPLAY_RWSET_PC_LEN);
            pcBuf[bytes] = '\0';
            uint64_t pc =  strtoul (pcBuf, NULL, 16);

            char upcBuf[REPLAY_RWSET_UPC_LEN+1]; // +1 for the \0
            bytes = 0;
            while (bytes < REPLAY_RWSET_UPC_LEN) {
                int read_bytes = read(fileDescriptor, &upcBuf[bytes],
                                      REPLAY_RWSET_UPC_LEN-bytes);
                if (read_bytes > 0) bytes += read_bytes;
            }
            assert(bytes == REPLAY_RWSET_UPC_LEN);
            upcBuf[bytes] = '\0';
            uint64_t upc =  strtoul (upcBuf, NULL, 16);

            char addrBuf[REPLAY_RWSET_ADDR_LEN+1]; // +1 for the \0
            bytes = 0;
            while (bytes < REPLAY_RWSET_ADDR_LEN) {
                int read_bytes = read(fileDescriptor, &addrBuf[bytes],
                                      REPLAY_RWSET_ADDR_LEN-bytes);
                if (read_bytes > 0) bytes += read_bytes;
            }
            assert(bytes == REPLAY_RWSET_ADDR_LEN);
            addrBuf[bytes] = '\0';
            uint64_t addr =  strtoul (addrBuf, NULL, 16);

            char dataBuf[REPLAY_RWSET_VALUE_LEN+1];
            //char dataBuf[REPLAY_RWSET_DATABLOCK_LEN];
            bytes = 0;
            while (bytes < REPLAY_RWSET_VALUE_LEN) {
                int read_bytes = read(fileDescriptor, &dataBuf[bytes],
                                      REPLAY_RWSET_VALUE_LEN-bytes);
                if (read_bytes > 0) bytes += read_bytes;
            }
            assert(bytes == REPLAY_RWSET_VALUE_LEN);
            dataBuf[bytes] = '\0';
            uint64_t value = strtoul (dataBuf, NULL, 16);

            replayValues
                .push_back(ValueRecord(wasStore,
                                       pc, upc,
                                       addr, value,
                                       seqNum));
            DPRINTF(Lockstep, "Replay record %ld for "
                    "transactional %s "
                    "addr: %#x value: %#x\n", seqNum,
                    wasStore ? "store" : " load", addr, value);
        }
    }
}

} // namespace gem5
