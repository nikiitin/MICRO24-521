
#include "sim/commit_order.hh"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <utility>

#include "debug/CommitOrder.hh"
#include "enums/LockStepMode.hh"
#include "sim/sim_exit.hh"

namespace gem5
{


/* Format of synchronization records: one record per line, first tid
   followed by xid, separated with one space, in hexadecimal

    <tid><space><xid><newline>
    Example:
     9  2\n  ===> thread 9, critical section 2)
    12 18\n  ===> thread 12, critical section 18)
    <tid> and <xid> may have different widths, set below
 */
#define COMMIT_ORDER_RECORD_TYPE_WIDTH (1)
#define COMMIT_ORDER_RECORD_TID_WIDTH (2) // Must be multiple of 2
#define COMMIT_ORDER_RECORD_TAG_WIDTH (2) // Must be multiple of 2
#define COMMIT_ORDER_RECORD_TYPE_INDEX (0)
#define COMMIT_ORDER_RECORD_TID_INDEX (COMMIT_ORDER_RECORD_TYPE_WIDTH + 1) // one space
#define COMMIT_ORDER_RECORD_TAG_INDEX (COMMIT_ORDER_RECORD_TYPE_WIDTH + \
                                       COMMIT_ORDER_RECORD_TID_WIDTH + 2) // two spaces
#define COMMIT_ORDER_RECORD_LEN (COMMIT_ORDER_RECORD_TYPE_WIDTH +       \
                                 COMMIT_ORDER_RECORD_TID_WIDTH +        \
                                 COMMIT_ORDER_RECORD_TAG_WIDTH +        \
                                 3) // two spaces + EOL

#define COMMIT_ORDER_RECORD_TYPE_EXIT ('e')
#define COMMIT_ORDER_RECORD_TYPE_INVALID ('i')

using namespace std;

CommitOrder::CommitOrder(System *_sys, int _fd) :
    system(_sys),
    fileDescriptor(_fd)
{
    nextCommitToReplay_type = COMMIT_ORDER_RECORD_TYPE_INVALID;
    //const System::Params *params = system->params();
    if (system->getKvmVM() != nullptr) {
        synchronized = true;
        //assert(params->in_fast_forward_phase);
    }
    else {
        synchronized = false;
        //assert(!params->in_fast_forward_phase);
    }
    //_unused(params);
}

bool
CommitOrder::isValid(char type, int tid, int tag) const {
    return type != COMMIT_ORDER_RECORD_TYPE_INVALID;
}

bool
CommitOrder::isEOF(char type, int tid, int tag)
{
  return (type == COMMIT_ORDER_RECORD_TYPE_EXIT);
}

void
CommitOrder::recordEOF(int fileDescriptor)
{
    record(COMMIT_ORDER_RECORD_TYPE_EXIT, -1, -1,
           fileDescriptor);
}


const string
CommitOrder::toString(char type, int tid, int tag) const
{
  stringstream sstream;

  // Ensure cpu/xid fit in fixed-width text field
  assert(tid < (pow(16,COMMIT_ORDER_RECORD_TID_WIDTH)));
  assert(tag < (pow(16,COMMIT_ORDER_RECORD_TAG_WIDTH)));
  sstream << type
          << ' '
          << hex
          << setw(COMMIT_ORDER_RECORD_TID_WIDTH) << tid
          << ' '
          << setw(COMMIT_ORDER_RECORD_TAG_WIDTH) << tag
          << endl;
  string str = sstream.str();
  assert(str.length() == COMMIT_ORDER_RECORD_LEN);
  return str;

}

void
CommitOrder::fromString(char *buf, char &type, int &tid, int &tag) const
{

    // Space separating type from tid and tid from work/sync id
    assert(buf[COMMIT_ORDER_RECORD_TID_INDEX-1] == ' ');
    assert(buf[COMMIT_ORDER_RECORD_TAG_INDEX-1] == ' ');
    assert(buf[COMMIT_ORDER_RECORD_LEN-1] == '\n');
    // Replace spaces and eol with NULL character for strtoul
    buf[COMMIT_ORDER_RECORD_TID_INDEX-1] = '\0';
    buf[COMMIT_ORDER_RECORD_TAG_INDEX-1] = '\0';
    buf[COMMIT_ORDER_RECORD_LEN-1] = '\0';
    type = buf[COMMIT_ORDER_RECORD_TYPE_INDEX];
    tid = strtoul (&buf[COMMIT_ORDER_RECORD_TID_INDEX], NULL, 16);
    tag = strtoul (&buf[COMMIT_ORDER_RECORD_TAG_INDEX], NULL, 16);
}

void
CommitOrder::record(char type, int tid, int tag,
                    int fileDescriptor)
{
  assert(fileDescriptor >= 0);

  const string str = toString(type, tid, tag);
  const char *buf = str.c_str();
  int bytes = 0;
  while (bytes < COMMIT_ORDER_RECORD_LEN) {
    int written_bytes = write(fileDescriptor,
                              (void *)&buf[bytes],
                              COMMIT_ORDER_RECORD_LEN - bytes);
    // Wait until pipe open in replay CPU
    if (written_bytes == -1) continue;
    if (written_bytes > 0) bytes += written_bytes;
  }
  if (isEOF(type, tid, tag)) {
    // This record signals the end of the commit order stream
    DPRINTF(CommitOrder, "lockstep mode: exit record dumped"
            " to commit order stream\n");
    return;
  }
  DPRINTF(CommitOrder, "recording global commit order for cpu %d, "
          "xid %d, type %c\n", tid, tag, type);
  assert(bytes == COMMIT_ORDER_RECORD_LEN); // Hacky
}

bool
CommitOrder::readFromFile(char &type, int &tid, int &tag,
                          int fileDescriptor) {
    assert(fileDescriptor >= 0);
    // Try and read from fifo first
    char buf[COMMIT_ORDER_RECORD_LEN+1];
    int bytes = 0;
    while (bytes < COMMIT_ORDER_RECORD_LEN) {
      int read_bytes = read(fileDescriptor,
                            (void *)&buf[bytes],
                            COMMIT_ORDER_RECORD_LEN - bytes);
      // Wait until pipe open in replay CPU
      if (read_bytes == -1 && bytes == 0) {
        // read returns -1 when it would have blocked but nonblocking
        // operation was requested (fifo opened with O_NONBLOCK); If
        // bytes 0: Nothing read from fifo, wait for O3CPU
        return false;
      }
      if (read_bytes > 0) bytes += read_bytes;
    }
    assert(bytes == COMMIT_ORDER_RECORD_LEN);

    // Set record
    fromString(buf, type, tid, tag);
    DPRINTF(CommitOrder, "read from file commit record"
            " cpu %d, xid %d, type %c\n", tid, tag, type);

    if (isEOF(type, tid, tag)) {
      exitSimLoop("lockstep mode: exit record reached "
                  "while replaying commit order", 0, curTick(), 0, true);
    }

    return true;
}

bool
CommitOrder::canReplay(char type, int tid, int tag,
                       int fileDescriptor)
{
  assert(fileDescriptor >= 0);
  // Check if we already have the next commit to replay
  if (!isValid(nextCommitToReplay_type,
               nextCommitToReplay_tid,
               nextCommitToReplay_tag)) {
      // Try to read from file into nextCommitToReplay
      if (!readFromFile(nextCommitToReplay_type,
                        nextCommitToReplay_tid,
                        nextCommitToReplay_tag,
                        fileDescriptor))
          return false;
  }
  assert(isValid(nextCommitToReplay_type,
                 nextCommitToReplay_tid,
                 nextCommitToReplay_tag));

  // If cpu matches nextCommit, allow xact to begin
  if (tid == nextCommitToReplay_tid) {
    // Sanity check: type must match
    assert(type == nextCommitToReplay_type);
    // Sanity check: transaction xid must match
    assert(tag == nextCommitToReplay_tag);
    DPRINTF(CommitOrder, "replaying global commit order for cpu %d, xid %d, "
            "type %c\n", tid, tag, type);

    return true;
  }
  else { // xbegin not allowed for this cpu
    return false;
  }
}

void
CommitOrder::markReplayed(char type, int tid, int tag)
{
    if (synchronized) { // Need synchronization
        // Guard destroyed (unlocked) when block of code exits
        std::lock_guard<std::mutex> guard(replayMutex);
        markReplayedPriv(type, tid, tag);
    }
    else {
        markReplayedPriv(type, tid, tag);
    }
}

void
CommitOrder::markReplayedPriv(char type, int tid, int tag)
{
    assert(nextCommitToReplay_type == type);
    assert(nextCommitToReplay_tid == tid);
    assert(nextCommitToReplay_tag == tag);

    // "Consume" this record: will read from fifo in next call
    nextCommitToReplay_type = COMMIT_ORDER_RECORD_TYPE_INVALID;
    DPRINTF(CommitOrder, "marked replayed commit record"
            " cpu %d, xid %d, type %c\n", tid, tag, type);
}

void
CommitOrder::record(char type, int tid, int tag)
{
    record(type, tid, tag, fileDescriptor);
}

bool
CommitOrder::canReplay(char type, int tid, int tag)
{
    if (synchronized) { // Need synchronization
        // Guard destroyed (unlocked) when block of code exits
        std::lock_guard<std::mutex> guard(replayMutex);
        return canReplayPriv(type, tid, tag);
    }
    else {
        return canReplayPriv(type, tid, tag);
    }
}

bool
CommitOrder::canReplayPriv(char type, int tid, int tag)
{
    return canReplay(type, tid, tag, fileDescriptor);
}

void
CommitOrder::recordEOF() {
    recordEOF(fileDescriptor);
}

} // namespace gem5
