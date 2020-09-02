/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cache.h"
#include "hash.h"

#include "event_recorder.h"
#ifndef EXTERNAL_CACHE_MODEL
#include "timing_event.h"
#endif
#include "zsim.h"

Cache::Cache(uint32_t _numLines, CC* _cc, CacheArray* _array, ReplPolicy* _rp, uint32_t _accLat, uint32_t _invLat, const g_string& _name)
    : cc(_cc), array(_array), rp(_rp), numLines(_numLines), accLat(_accLat), invLat(_invLat), name(_name) {}

const char* Cache::getName() {
    return name.c_str();
}

void Cache::setParents(uint32_t childId, const g_vector<MemObject*>& parents, Network* network) {
    cc->setParents(childId, parents, network);
}

void Cache::setChildren(const g_vector<BaseCache*>& children, Network* network) {
    cc->setChildren(children, network);
}

void Cache::initStats(AggregateStat* parentStat) {
    AggregateStat* cacheStat = new AggregateStat();
    cacheStat->init(name.c_str(), "Cache stats");
    initCacheStats(cacheStat);
    parentStat->append(cacheStat);
}

void Cache::initCacheStats(AggregateStat* cacheStat) {
    cc->initStats(cacheStat);
    array->initStats(cacheStat);
    rp->initStats(cacheStat);
}

uint64_t Cache::access(MemReq& req) {
    // if(rp->supportBypass()){
    
    //     uint64_t respCycle = req.cycle;
    //     bool skipAccess = cc->startAccess(req); //may need to skip access due to races (NOTE: may change req.type!)
    //     if (likely(!skipAccess)) {
    //         // bool normal = req.replaced_cache_line_addr == nullptr && req.replaced_block_id == nullptr;
    //         // printf("\taccess %lx, cache=%p， supportBypass=%d, ", req.lineAddr, array, rp->supportBypass());
    //         // if(req.replaced_cache_line_addr)
    //         //     if(req.replaced_block_id) printf("[non-fake]\n");
    //         //     else printf("[error]\n");
    //         // else
    //         //     if(req.replaced_block_id) printf("[fake]\n");
    //         //     else printf("[others/normal]\n");
            
    //         bool updateReplacement = (req.type == GETS) || (req.type == GETX);
    //         uint64_t availCycle;  //cycle the block is/will be available (valid only if MESI state is not I)
    //         int32_t lineId = array->lookup(req.lineAddr, &req, updateReplacement, &availCycle);
    //         if (lineId != -1 && cc->isValid(lineId)) {
    //             //If the block is still inbound, increase the delay.
    //             //This also fixes a (relatively infrequent) timing bug in the filter cache where the available cycle
    //             //info is lost if a block in a set is replaced and then used again before the cycle is reached.
    //             //Add accLat if a) line is in the cache or b) line is not in the cache. If c) line is in-flight (prefetch)
    //             //serve from MSHR and do not account for accLat
    //             respCycle = (availCycle > respCycle) ? availCycle : respCycle + accLat;
    //         }
    //         else { //Cache miss
    //             respCycle += accLat;
    //         }

    //         bool need_postinsert = false;
    //         // bool record=true;
    //         // bool record = req.replaced_cache_line_addr && *req.replaced_cache_line_addr; // if it is second access of bypass, ignore recording
    //         bool isfake = req.replaced_cache_line_addr == nullptr && req.replaced_block_id != nullptr;
    //         bool bypass = false;

    //         if (lineId == -1 && cc->shouldAllocate(req)) {
    //             //Make space for new line
    //             Address wbLineAddr;
                
    //             if(req.replaced_cache_line_addr){ // non-fake access
    //                 // printf("\tnon-fake %lx\n", req.lineAddr);
    //                 rp->recordStatus();
    //                 lineId = array->preinsert(req.lineAddr, &req, &wbLineAddr, &bypass); //find the lineId to replace
    //                 if(wbLineAddr == 0){
    //                     bypass = false;
    //                     assert(!isfake);
    //                 }
    //                 assert(req.replaced_block_id);
                    
    //                 if(!isfake && bypass){
    //                     *req.replaced_cache_line_addr = wbLineAddr;
    //                     *req.replaced_block_id = lineId;
    //                 }
    //                 // if(bypass)
    //                     // printf("\tbypass addr=%lx, id=%d\n", wbLineAddr, lineId);
    //             }else{ // fake access or no bypassing mechanism
    //                 // lineId = array->preinsert(req.lineAddr, &req, &wbLineAddr, nullptr, req.replaced_block_id);
    //                 lineId = array->preinsert(req.lineAddr, &req, &wbLineAddr, nullptr, nullptr);
    //                 // if(isfake) printf("fake %lx\n", wbLineAddr);
    //             }
    //             ZSIM_TRACE(Cache, "[%s] Evicting 0x%lx", name.c_str(), wbLineAddr);

    //             //Evictions are not in the critical path in any sane implementation -- we do not include their delays
    //             //NOTE: We might be "evicting" an invalid line for all we know. Coherence controllers will know what to do
    //             cc->processEviction(req, wbLineAddr, lineId, respCycle); //1. if needed, send invalidates/downgrades to lower level

    //             need_postinsert = true;  //defer the actual insertion until we know its cycle availability
    //         }
    // #ifndef EXTERNAL_CACHE_MODEL
    //         // Enforce single-record invariant: Writeback access may have a timing
    //         // record. If so, read it.
    //         EventRecorder* evRec = zinfo->eventRecorders[req.srcId];
    //         TimingRecord wbAcc;
    //         wbAcc.clear();
    //         if (unlikely(evRec && evRec->hasRecord() && req.prefetch == 0) && !isfake) {
    //             wbAcc = evRec->popRecord();
    //         }
    // #endif

    //         respCycle = cc->processAccess(req, lineId, respCycle);
    //         if (need_postinsert) {

    //             array->postinsert(req.lineAddr, &req, lineId, respCycle, true/*!isfake && !bypass*/); //do the actual insertion. NOTE: Now we must split insert into a 2-phase thing because cc unlocks us.
    //             if(!isfake && req.replaced_cache_line_addr && *req.replaced_cache_line_addr == 0)
    //                 rp->afterMiss(req.lineAddr, lineId);
    //             if(isfake){
    //                 // printf("\tchecking status...\n");
    //                 bool ok = rp->checkStatus();
    //                 assert(ok);
    //             }
    //         } else {
    //             rp->afterHit(req.lineAddr, lineId);
    //         }

    // #ifndef EXTERNAL_CACHE_MODEL
    //         // Access may have generated another timing record. If *both* access
    //         // and wb have records, stitch them together
    //         if (unlikely(wbAcc.isValid()) && !isfake) {
    //             if (!evRec->hasRecord()) {
    //                 // Downstream should not care about endEvent for PUTs
    //                 wbAcc.endEvent = nullptr;
    //                 evRec->pushRecord(wbAcc);
    //             } else {
    //                 // Connect both events
    //                 TimingRecord acc = evRec->popRecord();
    //                 assert(wbAcc.reqCycle >= req.cycle);
    //                 assert(acc.reqCycle >= req.cycle);
    //                 DelayEvent* startEv = new (evRec) DelayEvent(0);
    //                 DelayEvent* dWbEv = new (evRec) DelayEvent(wbAcc.reqCycle - req.cycle);
    //                 DelayEvent* dAccEv = new (evRec) DelayEvent(acc.reqCycle - req.cycle);
    //                 startEv->setMinStartCycle(req.cycle);
    //                 dWbEv->setMinStartCycle(req.cycle);
    //                 dAccEv->setMinStartCycle(req.cycle);
    //                 startEv->addChild(dWbEv, evRec)->addChild(wbAcc.startEvent, evRec);
    //                 startEv->addChild(dAccEv, evRec)->addChild(acc.startEvent, evRec);

    //                 acc.reqCycle = req.cycle;
    //                 acc.startEvent = startEv;
    //                 // endEvent / endCycle stay the same; wbAcc's endEvent not connected
    //                 evRec->pushRecord(acc);
    //             }
    //         }
    // #endif
    //     }

    //     cc->endAccess(req);

    //     assert_msg(respCycle >= req.cycle, "[%s] resp < req? 0x%lx type %s childState %s, respCycle %ld reqCycle %ld",
    //             name.c_str(), req.lineAddr, AccessTypeName(req.type), MESIStateName(*req.state), respCycle, req.cycle);
    //     return respCycle;
    
    // }else{
        // printf("\taccess %lx, cache=%p， supportBypass=%d\n", req.lineAddr, array, rp->supportBypass());
    
        uint64_t respCycle = req.cycle;
        bool skipAccess = cc->startAccess(req); //may need to skip access due to races (NOTE: may change req.type!)
        if (likely(!skipAccess)) {
            bool updateReplacement = (req.type == GETS) || (req.type == GETX);
            uint64_t availCycle;  //cycle the block is/will be available (valid only if MESI state is not I)
            int32_t lineId = array->lookup(req.lineAddr, &req, updateReplacement, &availCycle);
            if (lineId != -1 && cc->isValid(lineId)) {
                //If the block is still inbound, increase the delay.
                //This also fixes a (relatively infrequent) timing bug in the filter cache where the available cycle
                //info is lost if a block in a set is replaced and then used again before the cycle is reached.
                //Add accLat if a) line is in the cache or b) line is not in the cache. If c) line is in-flight (prefetch)
                //serve from MSHR and do not account for accLat
                respCycle = (availCycle > respCycle) ? availCycle : respCycle + accLat;
            }
            else { //Cache miss
                respCycle += accLat;
            }

            bool need_postinsert = false;

            if (lineId == -1 && cc->shouldAllocate(req)) {
                //Make space for new line
                Address wbLineAddr;
                lineId = array->preinsert(req.lineAddr, &req, &wbLineAddr); //find the lineId to replace
                ZSIM_TRACE(Cache, "[%s] Evicting 0x%lx", name.c_str(), wbLineAddr);

                //Evictions are not in the critical path in any sane implementation -- we do not include their delays
                //NOTE: We might be "evicting" an invalid line for all we know. Coherence controllers will know what to do
                cc->processEviction(req, wbLineAddr, lineId, respCycle); //1. if needed, send invalidates/downgrades to lower level

                need_postinsert = true;  //defer the actual insertion until we know its cycle availability
            }
    #ifndef EXTERNAL_CACHE_MODEL
            // Enforce single-record invariant: Writeback access may have a timing
            // record. If so, read it.
            EventRecorder* evRec = zinfo->eventRecorders[req.srcId];
            TimingRecord wbAcc;
            wbAcc.clear();
            if (unlikely(evRec && evRec->hasRecord() && req.prefetch == 0)) {
                wbAcc = evRec->popRecord();
            }
    #endif

            respCycle = cc->processAccess(req, lineId, respCycle);
            if (need_postinsert) {
                array->postinsert(req.lineAddr, &req, lineId, respCycle); //do the actual insertion. NOTE: Now we must split insert into a 2-phase thing because cc unlocks us.
            }

    #ifndef EXTERNAL_CACHE_MODEL
            // Access may have generated another timing record. If *both* access
            // and wb have records, stitch them together
            if (unlikely(wbAcc.isValid())) {
                if (!evRec->hasRecord()) {
                    // Downstream should not care about endEvent for PUTs
                    wbAcc.endEvent = nullptr;
                    evRec->pushRecord(wbAcc);
                } else {
                    // Connect both events
                    TimingRecord acc = evRec->popRecord();
                    assert(wbAcc.reqCycle >= req.cycle);
                    assert(acc.reqCycle >= req.cycle);
                    DelayEvent* startEv = new (evRec) DelayEvent(0);
                    DelayEvent* dWbEv = new (evRec) DelayEvent(wbAcc.reqCycle - req.cycle);
                    DelayEvent* dAccEv = new (evRec) DelayEvent(acc.reqCycle - req.cycle);
                    startEv->setMinStartCycle(req.cycle);
                    dWbEv->setMinStartCycle(req.cycle);
                    dAccEv->setMinStartCycle(req.cycle);
                    startEv->addChild(dWbEv, evRec)->addChild(wbAcc.startEvent, evRec);
                    startEv->addChild(dAccEv, evRec)->addChild(acc.startEvent, evRec);

                    acc.reqCycle = req.cycle;
                    acc.startEvent = startEv;
                    // endEvent / endCycle stay the same; wbAcc's endEvent not connected
                    evRec->pushRecord(acc);
                }
            }
    #endif
        }

        cc->endAccess(req);

        assert_msg(respCycle >= req.cycle, "[%s] resp < req? 0x%lx type %s childState %s, respCycle %ld reqCycle %ld",
                name.c_str(), req.lineAddr, AccessTypeName(req.type), MESIStateName(*req.state), respCycle, req.cycle);
        return respCycle;
    
    // }
}

void Cache::startInvalidate() {
    cc->startInv(); //note we don't grab tcc; tcc serializes multiple up accesses, down accesses don't see it
}

uint64_t Cache::finishInvalidate(const InvReq& req) {
    uint64_t availCycle;
    int32_t lineId = array->lookup(req.lineAddr, nullptr, false, &availCycle);
    assert_msg(lineId != -1, "[%s] Invalidate on non-existing address 0x%lx type %s lineId %d, reqWriteback %d", name.c_str(), req.lineAddr, InvTypeName(req.type), lineId, *req.writeback);
    uint64_t respCycle = req.cycle + invLat;
    ZSIM_TRACE(Cache, "[%s] Invalidate start 0x%lx type %s lineId %d, reqWriteback %d", name.c_str(), req.lineAddr, InvTypeName(req.type), lineId, *req.writeback);
    respCycle = cc->processInv(req, lineId, respCycle); //send invalidates or downgrades to children, and adjust our own state
    ZSIM_TRACE(Cache, "[%s] Invalidate end 0x%lx type %s lineId %d, reqWriteback %d, latency %ld", name.c_str(), req.lineAddr, InvTypeName(req.type), lineId, *req.writeback, respCycle - req.cycle);

    return respCycle;
}
