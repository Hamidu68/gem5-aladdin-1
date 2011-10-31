/*
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_RUBY_SYSTEM_PERSISTENTTABLE_HH__
#define __MEM_RUBY_SYSTEM_PERSISTENTTABLE_HH__

#include <iostream>

#include "base/hashmap.hh"
#include "mem/protocol/AccessType.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/system/MachineID.hh"

class PersistentTableEntry
{
  public:
    PersistentTableEntry() {}
    void print(std::ostream& out) const {}

    NetDest m_starving;
    NetDest m_marked;
    NetDest m_request_to_write;
};

class PersistentTable
{
  public:
    // Constructors
    PersistentTable();

    // Destructor
    ~PersistentTable();

    // Public Methods
    void persistentRequestLock(const Address& address, MachineID locker,
                               AccessType type);
    void persistentRequestUnlock(const Address& address, MachineID unlocker);
    bool okToIssueStarving(const Address& address, MachineID machID) const;
    MachineID findSmallest(const Address& address) const;
    AccessType typeOfSmallest(const Address& address) const;
    void markEntries(const Address& address);
    bool isLocked(const Address& addr) const;
    int countStarvingForAddress(const Address& addr) const;
    int countReadStarvingForAddress(const Address& addr) const;

    static void printConfig(std::ostream& out) {}

    void print(std::ostream& out) const;

  private:
    // Private copy constructor and assignment operator
    PersistentTable(const PersistentTable& obj);
    PersistentTable& operator=(const PersistentTable& obj);

    // Data Members (m_prefix)
    typedef m5::hash_map<Address, PersistentTableEntry> AddressMap;
    AddressMap m_map;
};

inline std::ostream&
operator<<(std::ostream& out, const PersistentTable& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

inline std::ostream&
operator<<(std::ostream& out, const PersistentTableEntry& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

#endif // __MEM_RUBY_SYSTEM_PERSISTENTTABLE_HH__
