/*
 * Copyright (c) 2001-2004 The Regents of The University of Michigan
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

#ifndef __BASE_DYN_INST_CC__
#define __BASE_DYN_INST_CC__

#include <iostream>
#include <string>
#include <sstream>

#include "base/cprintf.hh"
#include "base/trace.hh"

#include "arch/alpha/faults.hh"
#include "cpu/exetrace.hh"
#include "mem/mem_req.hh"

#include "cpu/base_dyn_inst.hh"
#include "cpu/beta_cpu/alpha_impl.hh"
#include "cpu/beta_cpu/alpha_full_cpu.hh"

using namespace std;

#define NOHASH
#ifndef NOHASH

#include "base/hashmap.hh"

unsigned int MyHashFunc(const BaseDynInst *addr)
{
  unsigned a = (unsigned)addr;
  unsigned hash = (((a >> 14) ^ ((a >> 2) & 0xffff))) & 0x7FFFFFFF;

  return hash;
}

typedef m5::hash_map<const BaseDynInst *, const BaseDynInst *, MyHashFunc> my_hash_t;
my_hash_t thishash;
#endif

/** This may need to be specific to an implementation. */
//int BaseDynInst<Impl>::instcount = 0;

//int break_inst = -1;

template <class Impl>
BaseDynInst<Impl>::BaseDynInst(MachInst machInst, Addr inst_PC,
                               Addr pred_PC, InstSeqNum seq_num,
                               FullCPU *cpu)
    : staticInst(machInst), traceData(NULL), cpu(cpu), xc(cpu->xcBase())
{
    DPRINTF(FullCPU, "DynInst: Creating new DynInst.\n");

    effAddr = MemReq::inval_addr;
    physEffAddr = MemReq::inval_addr;

    readyRegs = 0;

    seqNum = seq_num;

    specMemWrite = false;

    canIssue = false;
    issued = false;
    executed = false;
    canCommit = false;
    squashed = false;
    squashedInIQ = false;

    blockingInst = false;
    recoverInst = false;
    specMode = false;
    btbMissed = false;
    // Eventually make this a parameter.
    threadNumber = 0;
    // Also make this a parameter.
    specMode = true;
    // Also make this a parameter, or perhaps get it from xc or cpu.
    asid = 0;

    // Initialize the fault to be unimplemented opcode.
    fault = Unimplemented_Opcode_Fault;

    PC = inst_PC;
    nextPC = PC + sizeof(MachInst);
    predPC = pred_PC;

    // Make sure to have the renamed register entries set to the same
    // as the normal register entries.  It will allow the IQ to work
    // without any modifications.
    for (int i = 0; i < staticInst->numDestRegs(); i++)
    {
        _destRegIdx[i] = staticInst->destRegIdx(i);
    }

    for (int i = 0; i < staticInst->numSrcRegs(); i++)
    {
        _srcRegIdx[i] = staticInst->srcRegIdx(i);
        _readySrcRegIdx[i] = 0;
    }

    ++instcount;

//    assert(instcount < 50);

    DPRINTF(FullCPU, "DynInst: Instruction created.  Instcount=%i\n",
            instcount);
}

template <class Impl>
BaseDynInst<Impl>::BaseDynInst(StaticInstPtr<ISA> &_staticInst)
    : staticInst(_staticInst), traceData(NULL)
{
    effAddr = MemReq::inval_addr;
    physEffAddr = MemReq::inval_addr;

    specMemWrite = false;

    blockingInst = false;
    recoverInst = false;
    specMode = false;
    btbMissed = false;

    // Make sure to have the renamed register entries set to the same
    // as the normal register entries.  It will allow the IQ to work
    // without any modifications.
    for (int i = 0; i < staticInst->numDestRegs(); i++)
    {
        _destRegIdx[i] = staticInst->destRegIdx(i);
    }

    for (int i = 0; i < staticInst->numSrcRegs(); i++)
    {
        _srcRegIdx[i] = staticInst->srcRegIdx(i);
    }
}

template <class Impl>
BaseDynInst<Impl>::~BaseDynInst()
{
/*
    if (specMemWrite) {
        // Remove effects of this instruction from speculative memory
        xc->spec_mem->erase(effAddr);
    }
*/
    --instcount;
    DPRINTF(FullCPU, "DynInst: Instruction destroyed.  Instcount=%i\n",
            instcount);
}

template <class Impl>
FunctionalMemory *
BaseDynInst<Impl>::getMemory(void)
{
    return xc->mem;
}
/*
template <class Impl>
IntReg *
BaseDynInst<Impl>::getIntegerRegs(void)
{
    return (spec_mode ? xc->specIntRegFile : xc->regs.intRegFile);
}
*/
template <class Impl>
void
BaseDynInst<Impl>::prefetch(Addr addr, unsigned flags)
{
    // This is the "functional" implementation of prefetch.  Not much
    // happens here since prefetches don't affect the architectural
    // state.

    // Generate a MemReq so we can translate the effective address.
    MemReqPtr req = new MemReq(addr, xc, 1, flags);
    req->asid = asid;

    // Prefetches never cause faults.
    fault = No_Fault;

    // note this is a local, not BaseDynInst::fault
    Fault trans_fault = xc->translateDataReadReq(req);

    if (trans_fault == No_Fault && !(req->flags & UNCACHEABLE)) {
        // It's a valid address to cacheable space.  Record key MemReq
        // parameters so we can generate another one just like it for
        // the timing access without calling translate() again (which
        // might mess up the TLB).
        effAddr = req->vaddr;
        physEffAddr = req->paddr;
        memReqFlags = req->flags;
    } else {
        // Bogus address (invalid or uncacheable space).  Mark it by
        // setting the eff_addr to InvalidAddr.
        effAddr = physEffAddr = MemReq::inval_addr;
    }

    /**
     * @todo
     * Replace the disjoint functional memory with a unified one and remove
     * this hack.
     */
#ifndef FULL_SYSTEM
    req->paddr = req->vaddr;
#endif

    if (traceData) {
        traceData->setAddr(addr);
    }
}

template <class Impl>
void
BaseDynInst<Impl>::writeHint(Addr addr, int size, unsigned flags)
{
    // Need to create a MemReq here so we can do a translation.  This
    // will casue a TLB miss trap if necessary... not sure whether
    // that's the best thing to do or not.  We don't really need the
    // MemReq otherwise, since wh64 has no functional effect.
    MemReqPtr req = new MemReq(addr, xc, size, flags);
    req->asid = asid;

    fault = xc->translateDataWriteReq(req);

    if (fault == No_Fault && !(req->flags & UNCACHEABLE)) {
        // Record key MemReq parameters so we can generate another one
        // just like it for the timing access without calling translate()
        // again (which might mess up the TLB).
        effAddr = req->vaddr;
        physEffAddr = req->paddr;
        memReqFlags = req->flags;
    } else {
        // ignore faults & accesses to uncacheable space... treat as no-op
        effAddr = physEffAddr = MemReq::inval_addr;
    }

    storeSize = size;
    storeData = 0;
}

/**
 * @todo Need to find a way to get the cache block size here.
 */
template <class Impl>
Fault
BaseDynInst<Impl>::copySrcTranslate(Addr src)
{
    MemReqPtr req = new MemReq(src, xc, 64);
    req->asid = asid;

    // translate to physical address
    Fault fault = xc->translateDataReadReq(req);

    if (fault == No_Fault) {
        xc->copySrcAddr = src;
        xc->copySrcPhysAddr = req->paddr;
    } else {
        xc->copySrcAddr = 0;
        xc->copySrcPhysAddr = 0;
    }
    return fault;
}

/**
 * @todo Need to find a way to get the cache block size here.
 */
template <class Impl>
Fault
BaseDynInst<Impl>::copy(Addr dest)
{
    uint8_t data[64];
    FunctionalMemory *mem = xc->mem;
    assert(xc->copySrcPhysAddr || xc->misspeculating());
    MemReqPtr req = new MemReq(dest, xc, 64);
    req->asid = asid;

    // translate to physical address
    Fault fault = xc->translateDataWriteReq(req);

    if (fault == No_Fault) {
        Addr dest_addr = req->paddr;
        // Need to read straight from memory since we have more than 8 bytes.
        req->paddr = xc->copySrcPhysAddr;
        mem->read(req, data);
        req->paddr = dest_addr;
        mem->write(req, data);
    }
    return fault;
}

template <class Impl>
void
BaseDynInst<Impl>::dump()
{
    cprintf("T%d : %#08d `", threadNumber, PC);
    cout << staticInst->disassemble(PC);
    cprintf("'\n");
}

template <class Impl>
void
BaseDynInst<Impl>::dump(std::string &outstring)
{
    std::ostringstream s;
    s << "T" << threadNumber << " : 0x" << PC << " "
      << staticInst->disassemble(PC);

    outstring = s.str();
}


#if 0
template <class Impl>
Fault
BaseDynInst<Impl>::mem_access(mem_cmd cmd, Addr addr, void *p, int nbytes)
{
    Fault fault;

    // check alignments, even speculative this test should always pass
    if ((nbytes & nbytes - 1) != 0 || (addr & nbytes - 1) != 0) {
        for (int i = 0; i < nbytes; i++)
            ((char *) p)[i] = 0;

        // I added the following because according to the comment above,
        // we should never get here.  The comment lies
#if 0
        panic("unaligned access. Cycle = %n", curTick);
#endif
        return No_Fault;
    }

    MemReqPtr req = new MemReq(addr, thread, nbytes);
    switch(cmd) {
      case Read:
        fault = spec_mem->read(req, (uint8_t *)p);
        break;

      case Write:
        fault = spec_mem->write(req, (uint8_t *)p);
        if (fault != No_Fault)
            break;

        specMemWrite = true;
        storeSize = nbytes;
        switch(nbytes) {
          case sizeof(uint8_t):
            *(uint8_t)&storeData = (uint8_t *)p;
            break;
          case sizeof(uint16_t):
            *(uint16_t)&storeData = (uint16_t *)p;
            break;
          case sizeof(uint32_t):
            *(uint32_t)&storeData = (uint32_t *)p;
            break;
          case sizeof(uint64_t):
            *(uint64_t)&storeData = (uint64_t *)p;
            break;
        }
        break;

      default:
        fault = Machine_Check_Fault;
        break;
    }

    trace_mem(fault, cmd, addr, p, nbytes);

    return fault;
}

#endif

int
BaseDynInst<AlphaSimpleImpl>::instcount = 0;

// Forward declaration...
template BaseDynInst<AlphaSimpleImpl>;

#endif // __BASE_DYN_INST_CC__
