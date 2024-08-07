/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/LazyTransactionCommitArbiter.hh"

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/packet.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/RubySystem.hh"

#define CLASS_NS LazyTransactionCommitArbiter::

namespace gem5
{
namespace ruby
{

CLASS_NS
LazyTransactionCommitArbiter(TransactionInterfaceManager *xact_mgr,
                             int version,
                             string arbitration_policy) {
    m_version = version;
    m_xact_mgr = xact_mgr;
    m_validated = false;
    m_validating = false;
    m_policy = arbitration_policy;
}

CLASS_NS ~LazyTransactionCommitArbiter() {
}

void
CLASS_NS beginTransaction()
{
    assert(!m_validated);
    assert(!m_validating);
}

void
CLASS_NS restartTransaction(){
    if (m_policy == HtmPolicyStrings::token) {
        if (m_validating) {
            m_xact_mgr->getHTM()->removeCommitTokenRequest(m_version);
            DPRINTF(RubyHTM, "PROC %d removed commit token request\n",
                    m_version);
        } else if (m_validated) {
            m_xact_mgr->getHTM()->releaseCommitToken(m_version);
            DPRINTF(RubyHTM, "PROC %d released commit token, "
                    "validated transaction was aborted\n",
                    m_version);
        }
    }
    m_validated = false;
    m_validating = false;
}

void
CLASS_NS commitTransaction(){
    assert(m_validated);
    assert(!m_validating);
    if (m_policy == HtmPolicyStrings::token) {
        m_xact_mgr->getHTM()->releaseCommitToken(m_version);
        DPRINTF(RubyHTM, "PROC %d released commit token\n",
                m_version);
    }
    m_validated = false;
    m_validating = false;
}


bool
CLASS_NS shouldValidateTransaction()
{
    if (m_policy == HtmPolicyStrings::magic) {
        return true;
    } else if (m_policy == HtmPolicyStrings::token) {
        return true;
    } else {
        panic("Invalid lazy commit validation policy\n");
    }
}

void
CLASS_NS initiateValidateTransaction()
{
    assert(!m_validated);
    bool failed = true; // Set to false if validated
    if (m_policy == HtmPolicyStrings::magic) {
        m_validating = true;
        // Magic conflict detection at commit time
        std::vector<TransactionInterfaceManager*> mgrs =
            m_xact_mgr->getRemoteTransactionManagers();
        bool existsRemoteConflictingCommitter = false;
        TransactionInterfaceManager* new_committer = m_xact_mgr;
        for (int i=0; i < mgrs.size(); i++) {
            TransactionInterfaceManager* ongoing_committer=mgrs[i];
            if (ongoing_committer->
                getXactLazyCommitArbiter()->validated()) {
                // Already validated committer (guaranteed commit)
                if (ongoing_committer->hasConflictWith(new_committer) ||
                    new_committer->hasConflictWith(ongoing_committer)) {
                    // If this transaction has a conflict with an
                    // ongoing committer, it cannot validate: wait
                    // until ongoing committer ends, may be aborted as
                    // a result of committer invalidations
                    DPRINTF(RubyHTM, "PROC %d validation failed due to "
                            "conflict with proc %d\n", m_version, i);
                    existsRemoteConflictingCommitter = true;
                }
            }
        }
        if (!existsRemoteConflictingCommitter) {
            failed = false;
        }
    } else if (m_policy == HtmPolicyStrings::token) {
        if (!m_validating) {
            m_validating = true;
            m_xact_mgr->getHTM()->requestCommitToken(m_version);
            DPRINTF(RubyHTM, "PROC %d requested commit token\n",
                    m_version);
        }
        int owner = m_xact_mgr->getHTM()->getTokenOwner();
        if (owner == m_version) {
            failed = false;
        }
        DPRINTF(RubyHTM, "PROC %d %s commit token - "
                "owner is %d\n",
                m_version,
                failed ? "denied" : "granted",
                owner);
    } else {
        panic("initiateValidateTransaction not tested!\n");
    }
    if (!failed) {
        m_validating = false;
        m_validated = true;
    }
}

} // namespace ruby
} // namespace gem5
