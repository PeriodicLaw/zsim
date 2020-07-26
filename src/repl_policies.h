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

#ifndef REPL_POLICIES_H_
#define REPL_POLICIES_H_

#include <functional>
#include "bithacks.h"
#include "cache_arrays.h"
#include "coherence_ctrls.h"
#include "memory_hierarchy.h"
#include "mtrand.h"
#include <map>
#include <vector>
#include <fstream>
#include <utility>

extern std::map<std::string, std::map<uint64_t, std::vector<uint64_t>>*> all_future_counts; // trace_zsim.cpp
extern bool record_counts; // trace_zsim.cpp

inline void readCountFile(const char* countFile, std::map<uint64_t, std::vector<uint64_t>> &future_counts) {
    if(!record_counts){
        // read the count data for optimal replacement
        std::ifstream fs(countFile);
        if(!fs)
            panic("can not open %s",countFile);
        while (!fs.eof()){
            char s[40];
            fs.getline(s,40);
            if(fs.eof()) break;
            uint64_t pc = strtoull(s, nullptr, 16);
            future_counts[pc] = std::vector<uint64_t>();
            while(!fs.eof()){
                fs.getline(s,40);
                if(fs.eof()) break;
                if(strlen(s)==0) break;
                uint64_t time = strtoull(s,nullptr,10);
                future_counts[pc].push_back(time);
            }
        }
        fs.close();
    }
    std::string s(countFile);
    all_future_counts[s] = &future_counts;
}

/* Generic replacement policy interface. A replacement policy is initialized by the cache (by calling setTop/BottomCC) and used by the cache array. Usage follows two models:
 * - On lookups, update() is called if the replacement policy is to be updated on a hit
 * - On each replacement, rank() is called with the req and a list of replacement candidates.
 * - When the replacement is done, replaced() is called. (See below for more detail.)
 */
class ReplPolicy : public GlobAlloc {
    protected:
        CC* cc; //coherence controller, used to figure out whether candidates are valid or number of sharers

    public:
        ReplPolicy() : cc(nullptr) {}

        virtual void setCC(CC* _cc) {cc = _cc;}

        virtual void update(uint32_t id, const MemReq* req) = 0;
        virtual void replaced(uint32_t id) = 0;

        virtual uint32_t rankCands(const MemReq* req, SetAssocCands cands) = 0;
        virtual uint32_t rankCands(const MemReq* req, ZCands cands) = 0;
        virtual uint32_t rankCandsWithBypass(const MemReq* req, SetAssocCands cands, bool &bypass) {bypass = needBypass(req->lineAddr); return rankCands(req, cands);}

        virtual void initStats(AggregateStat* parent) {}
        
        virtual bool needBypass(uint64_t addr) {return false;}
        // virtual bool needBypassWithCands(uint64_t addr, const MemReq* req, SetAssocCands cands) {return needBypass(addr);}
        virtual void afterHit(uint64_t addr, uint32_t block_id) {/*printf("after hit %lx\n",addr);*/}
        virtual void afterMiss(uint64_t addr, uint32_t block_id) {/*printf("after miss %lx\n",addr);*/} // called when miss but not bypass
};

/* Add DECL_RANK_BINDINGS to each class that implements the new interface,
 * then implement a single, templated rank() function (see below for examples)
 * This way, we achieve a simple, single interface that is specialized transparently to each type of array
 * (this code is performance-critical)
 */
#define DECL_RANK_BINDING(T) uint32_t rankCands(const MemReq* req, T cands) { return rank(req, cands); }
#define DECL_RANK_BINDINGS DECL_RANK_BINDING(SetAssocCands); DECL_RANK_BINDING(ZCands);

/* Legacy support.
 * - On each replacement, the controller first calls startReplacement(), indicating the line that will be inserted;
 *   then it calls recordCandidate() for each candidate it finds; finally, it calls getBestCandidate() to get the
 *   line chosen for eviction. When the replacement is done, replaced() is called. The division of getBestCandidate()
 *   and replaced() happens because the former is called in preinsert(), and the latter in postinsert(). Note how the
 *   same restrictions on concurrent insertions extend to this class, i.e. startReplacement()/recordCandidate()/
 *   getBestCandidate() will be atomic, but there may be intervening update() calls between getBestCandidate() and
 *   replaced().
 */
class LegacyReplPolicy : public virtual ReplPolicy {
    protected:
        virtual void startReplacement(const MemReq* req) {} //many policies don't need it
        virtual void recordCandidate(uint32_t id) = 0;
        virtual uint32_t getBestCandidate() = 0;

    public:
        template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
            startReplacement(req);
            for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
                recordCandidate(*ci);
            }
            return getBestCandidate();
        }

        DECL_RANK_BINDINGS;
};

/* Plain ol' LRU, though this one is sharers-aware, prioritizing lines that have
 * sharers down in the hierarchy vs lines not shared by anyone.
 */
template <bool sharersAware>
class LRUReplPolicy : public ReplPolicy {
    protected:
        uint64_t timestamp; // incremented on each access
        uint64_t* array;
        uint32_t numLines;
        std::map<uint64_t, std::vector<uint64_t>> future_counts;

    public:
        explicit LRUReplPolicy(uint32_t _numLines, const char* countFile = nullptr) : timestamp(1), numLines(_numLines) {
            array = gm_calloc<uint64_t>(numLines);
            if(countFile)
                readCountFile(countFile, future_counts);
        }

        ~LRUReplPolicy() {
            gm_free(array);
        }

        void update(uint32_t id, const MemReq* req) {
            if(req->timestamp)
                future_counts[req->lineAddr].push_back(req->timestamp);
            array[id] = timestamp++;
        }

        void replaced(uint32_t id) {
            array[id] = 0;
        }

        template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
            uint32_t bestCand = -1;
            uint64_t bestScore = (uint64_t)-1L;
            for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
                uint32_t s = score(*ci);
                bestCand = (s < bestScore)? *ci : bestCand;
                bestScore = MIN(s, bestScore);
            }
            return bestCand;
        }

        DECL_RANK_BINDINGS;

    private:
        inline uint64_t score(uint32_t id) { //higher is least evictable
            //array[id] < timestamp always, so this prioritizes by:
            // (1) valid (if not valid, it's 0)
            // (2) sharers, and
            // (3) timestamp
            return (sharersAware? cc->numSharers(id) : 0)*timestamp + array[id]*cc->isValid(id);
        }
};

//This is VERY inefficient, uses LRU timestamps to do something that in essence requires a few bits.
//If you want to use this frequently, consider a reimplementation
class TreeLRUReplPolicy : public LRUReplPolicy<true> {
    private:
        uint32_t* candArray;
        uint32_t numCands;
        uint32_t candIdx;

    public:
        TreeLRUReplPolicy(uint32_t _numLines, uint32_t _numCands) : LRUReplPolicy<true>(_numLines), numCands(_numCands), candIdx(0) {
            candArray = gm_calloc<uint32_t>(numCands);
            if (numCands & (numCands-1)) panic("Tree LRU needs a power of 2 candidates, %d given", numCands);
        }

        ~TreeLRUReplPolicy() {
            gm_free(candArray);
        }

        void recordCandidate(uint32_t id) {
            candArray[candIdx++] = id;
        }

        uint32_t getBestCandidate() {
            assert(candIdx == numCands);
            uint32_t start = 0;
            uint32_t end = numCands;

            while (end - start > 1) {
                uint32_t pivot = start + (end - start)/2;
                uint64_t t1 = 0;
                uint64_t t2 = 0;
                for (uint32_t i = start; i < pivot; i++) t1 = MAX(t1, array[candArray[i]]);
                for (uint32_t i = pivot; i < end; i++)   t2 = MAX(t2, array[candArray[i]]);
                if (t1 > t2) start = pivot;
                else end = pivot;
            }
            //for (uint32_t i = 0; i < numCands; i++) printf("%8ld ", array[candArray[i]]);
            //info(" res: %d (%d %ld)", start, candArray[start], array[candArray[start]]);
            return candArray[start];
        }

        void replaced(uint32_t id) {
            candIdx = 0;
            array[id] = 0;
        }
};

//2-bit NRU, see A new Case for Skew-Associativity, A. Seznec, 1997
class NRUReplPolicy : public LegacyReplPolicy {
    private:
        //read-only
        uint32_t* array;
        uint32_t* candArray;
        uint32_t numLines;
        uint32_t numCands;

        //read-write
        uint32_t youngLines;
        uint32_t candVal;
        uint32_t candIdx;

    public:
        NRUReplPolicy(uint32_t _numLines, uint32_t _numCands) :numLines(_numLines), numCands(_numCands), youngLines(0), candIdx(0) {
            array = gm_calloc<uint32_t>(numLines);
            candArray = gm_calloc<uint32_t>(numCands);
            candVal = (1<<20);
        }

        ~NRUReplPolicy() {
            gm_free(array);
            gm_free(candArray);
        }

        void update(uint32_t id, const MemReq* req) {
            //if (array[id]) info("update PRE %d %d %d", id, array[id], youngLines);
            youngLines += 1 - (array[id] >> 1); //+0 if young, +1 if old
            array[id] |= 0x2;

            if (youngLines >= numLines/2) {
                //info("youngLines = %d, shifting", youngLines);
                for (uint32_t i = 0; i < numLines; i++) array[i] >>= 1;
                youngLines = 0;
            }
            //info("update POST %d %d %d", id, array[id], youngLines);
        }

        void recordCandidate(uint32_t id) {
            uint32_t iVal = array[id];
            if (iVal < candVal) {
                candVal = iVal;
                candArray[0] = id;
                candIdx = 1;
            } else if (iVal == candVal) {
                candArray[candIdx++] = id;
            }
        }

        uint32_t getBestCandidate() {
            assert(candIdx > 0);
            return candArray[youngLines % candIdx]; // youngLines used to sort-of-randomize
        }

        void replaced(uint32_t id) {
            //info("repl %d val %d cands %d", id, array[id], candIdx);
            candVal = (1<<20);
            candIdx = 0;
            array[id] = 0;
        }
};

class RandReplPolicy : public LegacyReplPolicy {
    private:
        //read-only
        uint32_t* candArray;
        uint32_t numCands;

        //read-write
        MTRand rnd;
        uint32_t candVal;
        uint32_t candIdx;

    public:
        explicit RandReplPolicy(uint32_t _numCands) : numCands(_numCands), rnd(0x23A5F + (uint64_t)this), candIdx(0) {
            candArray = gm_calloc<uint32_t>(numCands);
        }

        ~RandReplPolicy() {
            gm_free(candArray);
        }

        void update(uint32_t id, const MemReq* req) {}

        void recordCandidate(uint32_t id) {
            candArray[candIdx++] = id;
        }

        uint32_t getBestCandidate() {
            assert(candIdx == numCands);
            uint32_t idx = rnd.randInt(numCands-1);
            return candArray[idx];
        }

        void replaced(uint32_t id) {
            candIdx = 0;
        }
};

class LFUReplPolicy : public LegacyReplPolicy {
    private:
        uint64_t timestamp; // incremented on each access
        int32_t bestCandidate; // id
        struct LFUInfo {
            uint64_t ts;
            uint64_t acc;
        };
        LFUInfo* array;
        uint32_t numLines;

        //NOTE: Rank code could be shared across Replacement policy implementations
        struct Rank {
            LFUInfo lfuInfo;
            uint32_t sharers;
            bool valid;

            void reset() {
                valid = false;
                sharers = 0;
                lfuInfo.ts = 0;
                lfuInfo.acc = 0;
            }

            inline bool lessThan(const Rank& other, const uint64_t curTs) const {
                if (!valid && other.valid) {
                    return true;
                } else if (valid == other.valid) {
                    if (sharers == 0 && other.sharers > 0) {
                        return true;
                    } else if (sharers > 0 && other.sharers == 0) {
                        return false;
                    } else {
                        if (lfuInfo.acc == 0) return true;
                        if (other.lfuInfo.acc == 0) return false;
                        uint64_t ownInvFreq = (curTs - lfuInfo.ts)/lfuInfo.acc; //inverse frequency, lower is better
                        uint64_t otherInvFreq = (curTs - other.lfuInfo.ts)/other.lfuInfo.acc;
                        return ownInvFreq > otherInvFreq;
                    }
                }
                return false;
            }
        };

        Rank bestRank;

    public:
        explicit LFUReplPolicy(uint32_t _numLines) : timestamp(1), bestCandidate(-1), numLines(_numLines) {
            array = gm_calloc<LFUInfo>(numLines);
            bestRank.reset();
        }

        ~LFUReplPolicy() {
            gm_free(array);
        }

        void update(uint32_t id, const MemReq* req) {
            //ts is the "center of mass" of all the accesses, i.e. the average timestamp
            array[id].ts = (array[id].acc*array[id].ts + timestamp)/(array[id].acc + 1);
            array[id].acc++;
            timestamp += 1000; //have larger steps to avoid losing too much resolution over successive divisions
        }

        void recordCandidate(uint32_t id) {
            Rank candRank = {array[id], cc? cc->numSharers(id) : 0, cc->isValid(id)};

            if (bestCandidate == -1 || candRank.lessThan(bestRank, timestamp)) {
                bestRank = candRank;
                bestCandidate = id;
            }
        }

        uint32_t getBestCandidate() {
            assert(bestCandidate != -1);
            return (uint32_t)bestCandidate;
        }

        void replaced(uint32_t id) {
            bestCandidate = -1;
            bestRank.reset();
            array[id].acc = 0;
        }
};

//Extends a given replacement policy to profile access ordering violations
template <class T>
class ProfViolReplPolicy : public T {
    private:
        struct AccTimes {
            uint64_t read;
            uint64_t write;
        };

        AccTimes* accTimes;

        Counter profRAW, profWAR, profRAR, profWAW, profNoViolAcc;
        Counter profAAE, profNoViolEv; //access after eviction violation

        uint64_t replCycle;

    public:
        //using T::T; //C++11, but can't do in gcc yet

        //Since this is only used with LRU, let's do that...
        explicit ProfViolReplPolicy(uint32_t nl) : T(nl) {}

        void init(uint32_t numLines) {
            accTimes = gm_calloc<AccTimes>(numLines);
            replCycle = 0;
        }

        void initStats(AggregateStat* parentStat) {
            T::initStats(parentStat);
            profRAW.init("vRAW", "RAW violations (R simulated before preceding W)");
            profWAR.init("vWAR", "WAR violations (W simulated before preceding R)");
            profRAR.init("vRAR", "RAR violations (R simulated before preceding R)");
            profWAW.init("vWAW", "WAW violations (W simulated before preceding W)");
            profAAE.init("vAAE", "Access simulated before preceding eviction");
            profNoViolAcc.init("noViolAcc", "Accesses without R/WAR/W violations");
            profNoViolEv.init("noViolEv",  "Evictions without AAE violations");

            parentStat->append(&profRAW);
            parentStat->append(&profWAR);
            parentStat->append(&profRAR);
            parentStat->append(&profWAW);
            parentStat->append(&profAAE);
            parentStat->append(&profNoViolAcc);
            parentStat->append(&profNoViolEv);
        }

        void update(uint32_t id, const MemReq* req) {
            T::update(id, req);

            bool read = (req->type == GETS);
            assert(read || req->type == GETX);
            uint64_t cycle = req->cycle;

            if (cycle < MAX(accTimes[id].read, accTimes[id].write)) { //violation
                //Now have to determine order
                bool readViol;
                if (cycle < MIN(accTimes[id].read, accTimes[id].write)) { //before both
                    readViol = (accTimes[id].read < accTimes[id].write); //read is closer
                } else if (cycle < accTimes[id].read) { //write, current access, read -> XAR viol
                    readViol = true;
                } else { //read, current access, write -> XAW viol
                    assert(cycle < accTimes[id].write);
                    readViol = false;
                }

                //Record
                read? (readViol? profRAR.inc() : profRAW.inc()) : (readViol? profWAR.inc() : profWAW.inc());

                //info("0x%lx viol read %d readViol %d cycles: %ld | r %ld w %ld", req->lineAddr, read, readViol, cycle, accTimes[id].read, accTimes[id].write);
            } else {
                profNoViolAcc.inc();
            }

            //Record
            if (read) accTimes[id].read  = MAX(accTimes[id].read,  req->cycle);
            else      accTimes[id].write = MAX(accTimes[id].write, req->cycle);

            T::update(id, req);
        }

        void startReplacement(const MemReq* req) {
            T::startReplacement(req);

            replCycle = req->cycle;
        }

        void replaced(uint32_t id) {
            T::replaced(id);

            if (replCycle < MAX(accTimes[id].read, accTimes[id].write)) {
                profAAE.inc();
            } else {
                profNoViolEv.inc();
            }

            //Reset --- update() will set correctly
            accTimes[id].read = 0;
            accTimes[id].write = 0;
        }
};

class OptReplPolicy : public ReplPolicy {
    protected:
        // uint64_t timestamp; // incremented on each access
        uint64_t* array;
        uint32_t numLines;
        std::map<uint64_t, std::vector<uint64_t>> future_counts;

    public:
        explicit OptReplPolicy(uint32_t _numLines, const char* countFile) : numLines(_numLines) {
            if(record_counts)
                panic("no record counts file offered, you cannot use opt repl")
            array = gm_calloc<uint64_t>(numLines);
            if(countFile)
                readCountFile(countFile, future_counts);
        }

        ~OptReplPolicy() {
            gm_free(array);
        }

        void update(uint32_t id, const MemReq* req) {
            // when not recording, records are decreasing
            while(!future_counts[req->lineAddr].empty() && future_counts[req->lineAddr].back() <= req->timestamp)
                future_counts[req->lineAddr].pop_back();
            array[id] = req->lineAddr;
        }

        void replaced(uint32_t id) {
            array[id] = 0;
        }

        template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
            
            uint32_t bestCand = -1;
            // uint64_t bestScore = (uint64_t)-1L;
            uint64_t bestScore = 0;
            for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
                auto id = *ci;
                if(!cc->isValid(id))
                    return id;
                
                bool ok = future_counts.find(array[id])!=future_counts.end();
                assert(ok);
                if(req->timestamp)
                    while(!future_counts[array[id]].empty() && future_counts[array[id]].back() <= req->timestamp)
                        future_counts[array[id]].pop_back();
                
                if(future_counts[array[id]].empty())
                    return id;
                auto s = future_counts[array[id]].back();
                if(s>bestScore){
                    bestCand = id;
                    bestScore = s;
                }
            }
            // printf("no invalid, that's good\n");
            return bestCand;
        }

        DECL_RANK_BINDINGS;

    // private:
    //     inline uint64_t score(uint32_t id) { //higher is least evictable
    //         if(!cc->isValid(id)) return 0;
    //         return future_counts[array[id]].back();
    //     }
};


class OptBypassPolicy : public OptReplPolicy {
    protected:
        std::ofstream replace_record_file;

    public:
        explicit OptBypassPolicy(uint32_t _numLines, const char* countFile) : OptReplPolicy(_numLines, countFile), replace_record_file("replace_record.txt") {}
        
        uint32_t rankCandsWithBypass(const MemReq* req, SetAssocCands cands, bool &bypass) override {
            auto addr = req->lineAddr;
            bypass = false;
            replace_record_file << std::hex << addr << ", ";
            
            uint32_t bestCand = -1;
            // uint64_t bestScore = (uint64_t)-1L;
            uint64_t bestScore = 0;
            uint64_t bestAddr;
            for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
                auto id = *ci;
                if(!cc->isValid(id)){
                    replace_record_file << "yes, 0" << std::endl;
                    return id;
                }
                
                bool ok = future_counts.find(array[id])!=future_counts.end();
                assert(ok);
                if(req->timestamp)
                    while(!future_counts[array[id]].empty() && future_counts[array[id]].back() <= req->timestamp)
                        future_counts[array[id]].pop_back();
                
                if(future_counts[array[id]].empty()){
                    replace_record_file << "yes, " << array[id] << std::endl;
                    return id;
                }
                auto s = future_counts[array[id]].back();
                if(s>bestScore){
                    bestCand = id;
                    bestScore = s;
                    bestAddr = array[id];
                }
            }
            
            bool ok = future_counts.find(addr)!=future_counts.end();
            assert(ok);
            while(!future_counts[addr].empty() && future_counts[addr].back() <= req->timestamp)
                future_counts[addr].pop_back();
            if(future_counts[addr].empty() || future_counts[addr].back() > bestScore){ // if request addr is the most future, bypass
                bypass = true;
                replace_record_file << "no" << std::endl;
            } else
                replace_record_file << "yes, " << bestAddr << std::endl;
            return bestCand;
        }
};


class GHRPReplPolicy : public ReplPolicy {
    protected:
        uint64_t timestamp;
        uint64_t* array;
        uint32_t numLines;
        uint16_t* block_signature;
        bool* block_dead;
        uint16_t global_history;
        const static int numCounts=4096;
        const static int numPredTables=3;
        const static uint64_t bypassThresh=20, deadThresh=20;
        uint64_t predTables[numCounts][numPredTables];

    public:
        explicit GHRPReplPolicy(uint32_t _numLines) : timestamp(1), numLines(_numLines) {
            array = gm_calloc<uint64_t>(numLines);
            block_signature = gm_calloc<uint16_t>(numLines);
            block_dead = gm_calloc<bool>(numLines);
            global_history = 0;
        }

        ~GHRPReplPolicy() {
            gm_free(array);
            gm_free(block_signature);
            gm_free(block_dead);
        }

        void update(uint32_t id, const MemReq* req) {
            array[id] = timestamp++;
        }

        void replaced(uint32_t id) {
            array[id] = 0;
        }
        
        void afterAccess(uint64_t addr, uint32_t block_id){
            block_signature[block_id] = make_signature(addr, global_history);
            update_history(addr, global_history);
        }
        
        bool needBypass(uint64_t addr) override {
            // return false;
            auto sign = make_signature(addr, global_history);
            auto cntrs = getCounters(computeIndices(sign));
            return majorityVote(cntrs, bypassThresh);
        }
        
        void afterHit(uint64_t addr, uint32_t block_id) override {
            // printf("afterHit %lx\n",addr);
            auto sign = make_signature(addr, global_history);
            auto cntrs = getCounters(computeIndices(sign));
            updatePredTable(computeIndices(block_signature[block_id]), false);
            block_dead[block_id] = majorityVote(cntrs, deadThresh);
            afterAccess(addr, block_id);
        }
        
        void afterMiss(uint64_t addr, uint32_t block_id) override {
            // printf("afterMiss %lx\n",addr);
            auto sign = make_signature(addr, global_history);
            auto cntrs = getCounters(computeIndices(sign));
            updatePredTable(computeIndices(block_signature[block_id]), true);
            block_dead[block_id] = majorityVote(cntrs, deadThresh);
            afterAccess(addr, block_id);
        }

        template <typename C> inline uint32_t rank(const MemReq* req, C cands) {
            uint32_t bestCand = -1;
            uint64_t bestScore = (uint64_t)-1L;
            for (auto ci = cands.begin(); ci != cands.end(); ci.inc()) {
                auto id = *ci;
                if(block_dead[id])
                    return id;
                else {
                    uint32_t s = score(id);
                    bestCand = (s < bestScore)? id : bestCand;
                    bestScore = MIN(s, bestScore);
                }
            }
            // if(needBypass(req->lineAddr)){
            //     req->replaced_cache_line_addr = ;
            // }
            return bestCand;
        }

        DECL_RANK_BINDINGS;

    private:
        inline uint64_t score(uint32_t id) {
            return array[id]*cc->isValid(id);
        }
        
        inline uint16_t make_signature(uint64_t pc, uint16_t his) {
            return (uint16_t)(pc ^ his);
        }
        
        inline void update_history(uint64_t pc, uint16_t &his) {
            his = (his << 4) | (pc & 7);
        }
        
        inline std::vector<uint64_t> computeIndices(uint16_t signature) {
            std::vector<uint64_t> indices;
            for(int i=0; i<numPredTables; i++)
                indices.push_back(hash(signature,i));
            return indices;
        }
        
        inline std::vector<uint64_t> getCounters(std::vector<uint64_t> indices) {
            std::vector<uint64_t> counters;
            for(int t=0; t<numPredTables; t++)
                counters.push_back(predTables[indices[t]][t]);
            return counters;
        }
        
        inline bool majorityVote(std::vector<uint64_t> counters, uint64_t threshold) {
            int vote = 0;
            for(int i=0; i<numPredTables; i++)
                if(counters[i] > threshold)
                    vote++;
            return (vote*2 >= numPredTables);
        }
        
        inline void updatePredTable(std::vector<uint64_t> indices, bool isDead) {
            for(int t=0; t<numPredTables; t++)
                if(isDead)
                    predTables[indices[t]][t]++;
                else
                    predTables[indices[t]][t]--;
        }
        
        typedef uint64_t UINT64;
        inline UINT64 mix(UINT64 a, UINT64 b, UINT64 c) {
            a -= b; a -= c; a ^= (c>>13);
            b -= c; b -= a; b ^= (a<<8);
            c -= a; c -= b; c ^= (b>>13);
            a -= b; a -= c; a ^= (c>>12);
            b -= c; b -= a; b ^= (a<<16);
            c -= a; c -= b; c ^= (b>>5);
            a -= b; a -= c; a ^= (c>>3);
            b -= c; b -= a; b ^= (a<<10);
            c -= a; c -= b; c ^= (b>>15);
            return c;
        }
        inline UINT64 f1(UINT64 x) {
            UINT64 fone = mix(0xfeedface, 0xdeadb10c, x);
            return fone;
        }
        inline UINT64 f2(UINT64 x) {
            UINT64 ftwo = mix(0xc001d00d, 0xfade2b1c, x);
            return ftwo;
        }
        inline UINT64 fi(UINT64 x) {
            UINT64 ind = (f1(x) )+ (f2(x));
            return ind ;
        }
        inline uint64_t hash(uint16_t signature, int i) {
            if(i==0) return f1(signature) & (numCounts-1);
            else if(i==1) return f2(signature) & (numCounts-1);
            else if(i==2) return fi(signature) & (numCounts-1);
            else assert(false);
        }
        
};

#endif  // REPL_POLICIES_H_
