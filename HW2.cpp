/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <chrono>

using std::cerr;
using std::endl;
using std::fixed;
using std::left;
using std::right;
using std::setprecision;
using std::setw;
using std::string;

#define BILLION 1000000000

/* ================================================================== */
// Custom Structures
/* ================================================================== */

enum BranchType
{
    ForwardBranch = 0,
    BackwardBranch = 1
};

typedef struct _MisPrediction
{
    UINT64 Count = 0;
    UINT64 A_static_FNBT = 0;
    UINT64 B_bimodal = 0;
    UINT64 C_SAg = 0;
    UINT64 D_GAg = 0;
    UINT64 E_gshare = 0;
    UINT64 F_SAg_GAg = 0;
    UINT64 G1_SAg_GAg_gshare_majority = 0;
    UINT64 G2_SAg_GAg_gshare_tournament = 0;

    _MisPrediction operator+(const _MisPrediction &rhs)
    {
        _MisPrediction result;
        result.Count = this->Count + rhs.Count;
        result.A_static_FNBT = this->A_static_FNBT + rhs.A_static_FNBT;
        result.B_bimodal = this->B_bimodal + rhs.B_bimodal;
        result.C_SAg = this->C_SAg + rhs.C_SAg;
        result.D_GAg = this->D_GAg + rhs.D_GAg;
        result.E_gshare = this->E_gshare + rhs.E_gshare;
        result.F_SAg_GAg = this->F_SAg_GAg + rhs.F_SAg_GAg;
        result.G1_SAg_GAg_gshare_majority = this->G1_SAg_GAg_gshare_majority + rhs.G1_SAg_GAg_gshare_majority;
        result.G2_SAg_GAg_gshare_tournament = this->G2_SAg_GAg_gshare_tournament + rhs.G2_SAg_GAg_gshare_tournament;
        return result;
    }
} MisPrediction;

template <UINT8 K>
class SaturatingCounter
{
private:
    UINT64 value;

public:
    SaturatingCounter() : value(0) {}
    inline void increment()
    {
        if (value < (1 << K))
            value++;
    }
    inline void decrement()
    {
        if (value > 0)
            value--;
    }
    inline bool isTaken() const
    {
        return value >= (1 << (K - 1));
    }
};

typedef struct _BTBEntry
{
    UINT64 tag;
    UINT64 target;
    UINT64 LRUState = 0;
    BOOL valid = false;
} BTBEntry;

#define BTB_WAYS 4
class BTBSet
{
    BTBEntry entries[BTB_WAYS];

public:
    BTBSet() {}
    inline BTBEntry *find(UINT32 tag)
    {
        for (UINT32 i = 0; i < BTB_WAYS; i++)
        {
            if (entries[i].valid && entries[i].tag == tag)
            {
                entries[i].LRUState = 0;
                for (UINT32 j = 0; j < BTB_WAYS; j++)
                {
                    if (j != i && entries[j].valid)
                        entries[j].LRUState++;
                }
                return &entries[i];
            }
        }
        return NULL;
    }

    inline VOID insert(UINT64 tag, UINT64 target)
    {
        UINT64 maxLRUState = 0;
        UINT32 maxLRUStateIndex = 0;
        for (UINT32 i = 0; i < BTB_WAYS; i++)
        {
            if (!entries[i].valid)
            {
                entries[i].valid = true;
                entries[i].tag = tag;
                entries[i].target = target;
                entries[i].LRUState = 0;
                return;
            }
            else if (entries[i].LRUState > maxLRUState)
            {
                maxLRUState = entries[i].LRUState;
                maxLRUStateIndex = i;
            }
        }
        entries[maxLRUStateIndex].tag = tag;
        entries[maxLRUStateIndex].target = target;
        entries[maxLRUStateIndex].LRUState = 0;
    }
};

/* ================================================================== */
// Global variables
/* ================================================================== */
// UTILS
UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 fastForward = 0;
std::ostream *out = &cerr;
std::chrono::time_point<std::chrono::system_clock> startTime;

// PART A
// 0: Forwardbranch, 1: BackwardBranch
MisPrediction misPrediction[2];

// Datastructures for predictors
#define BIMODAL_PHT_SIZE 512
#define BIMODAL_PHT_BITS 2
// B. Bimodal predictor: 512x2-bit PHT
SaturatingCounter<BIMODAL_PHT_BITS> bimodalPHT[BIMODAL_PHT_SIZE];

#define SAG_PHT_SIZE 512
#define SAG_PHT_BITS 2
#define SAG_BHT_SIZE 1024
#define SAG_BHT_BITS 9
#define SAG_BHT_MASK ((1 << SAG_BHT_BITS) - 1)
UINT64 sagBHT[SAG_BHT_SIZE];
SaturatingCounter<SAG_PHT_BITS> sagPHT[SAG_PHT_SIZE];

#define GHR_BITS 9
#define GHR_MASK ((1 << GHR_BITS) - 1)
UINT64 GHR_9BIT = 0;

#define GAG_PHT_SIZE 512
#define GAG_PHT_BITS 3
SaturatingCounter<GAG_PHT_BITS> gagPHT[GAG_PHT_SIZE];

#define GSHARE_PHT_SIZE 512
#define GSHARE_PHT_BITS 3
SaturatingCounter<GSHARE_PHT_BITS> gsharePHT[GSHARE_PHT_SIZE];

#define HYBRID_MP_SIZE 512
#define HYBRID_MP_BITS 2
SaturatingCounter<HYBRID_MP_BITS> sagGagMetaPred[HYBRID_MP_SIZE];
SaturatingCounter<HYBRID_MP_BITS> sagGshareMetaPred[HYBRID_MP_SIZE];
SaturatingCounter<HYBRID_MP_BITS> gagGshareMetaPred[HYBRID_MP_SIZE];

// PART B
#define BTB_SETS 128
BTBSet btbPC[BTB_SETS];
BTBSet btbPCGHR[BTB_SETS];
// Counts
UINT64 btbLookupCount = 0;
UINT64 btbPCMissCount = 0;
UINT64 btbPCGHRTotalCount = 0;
#define BTB_PC_GHR_BITS 7
#define BTB_PC_GHR_MASK ((1 << BTB_PC_GHR_BITS) - 1)

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string>
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                             "fast forward this many billion instructions before starting to collect data");

/* ===================================================================== */
// Utilities
/* ===================================================================== */
#define LEFT_FMT setw(48) << left
#define METRICS_FMT(value, total)                                            \
    setw(9) << right << value << " (" << fixed << setprecision(2) << setw(5) \
            << right << (100.0 * value / total) << "%)" << endl

INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl
         << "instructions, basic blocks and threads in the application." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

VOID PrintMisprictionMetrics(MisPrediction _mp)
{
    *out << LEFT_FMT << "Total number of Unconditional branches: " << setw(9) << right << _mp.Count << endl;
    *out << LEFT_FMT << "A. Static FNBT: " << METRICS_FMT(_mp.A_static_FNBT, _mp.Count);
    *out << LEFT_FMT << "B. Bimodal predictor: " << METRICS_FMT(_mp.B_bimodal, _mp.Count);
    *out << LEFT_FMT << "C. SAg: " << METRICS_FMT(_mp.C_SAg, _mp.Count);
    *out << LEFT_FMT << "D. GAg: " << METRICS_FMT(_mp.D_GAg, _mp.Count);
    *out << LEFT_FMT << "E. gshare: " << METRICS_FMT(_mp.E_gshare, _mp.Count);
    *out << LEFT_FMT << "F. Hybrid of SAg and GAg: " << METRICS_FMT(_mp.F_SAg_GAg, _mp.Count);
    *out << LEFT_FMT << "G. Hybrid of SAg, GAg, and gshare (majority): " << METRICS_FMT(_mp.G1_SAg_GAg_gshare_majority, _mp.Count);
    *out << LEFT_FMT << "G. Hybrid of SAg, GAg, and gshare (tournament): " << METRICS_FMT(_mp.G2_SAg_GAg_gshare_tournament, _mp.Count);
    *out << endl;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

#define FNBT_PREDICTION(target, iaddr) (target < iaddr)

VOID AnalyzeUncondBranch(ADDRINT iaddr, BOOL taken, ADDRINT target)
{
    BranchType branchType = target > iaddr ? ForwardBranch : BackwardBranch;
    misPrediction[branchType].Count++;
    UINT64 index;
    BOOL sagPrediction, gagPrediction, gsharePrediction, sagGagHybridPrediction, hybridPrediction;
    SaturatingCounter<HYBRID_MP_BITS> *sagGagGshareMetaPredEntry;

    // A. Static FNBT
    if (FNBT_PREDICTION(target, iaddr) != taken)
        misPrediction[branchType].A_static_FNBT++;

    // B. Bimodal predictor: 512x2-bit PHT
    index = (UINT64)iaddr % BIMODAL_PHT_SIZE;
    if (bimodalPHT[index].isTaken() != taken)
        misPrediction[branchType].B_bimodal++;
    if (taken)
        bimodalPHT[index].increment();
    else
        bimodalPHT[index].decrement();

    // C. SAg
    index = (UINT64)iaddr % SAG_BHT_SIZE;
    sagPrediction = sagPHT[sagBHT[index]].isTaken();
    if (sagPrediction != taken)
        misPrediction[branchType].C_SAg++;
    if (taken)
        sagPHT[sagBHT[index]].increment();
    else
        sagPHT[sagBHT[index]].decrement();
    sagBHT[index] = ((sagBHT[index] << 1) | taken) & SAG_BHT_MASK;

    // D. GAg
    index = GHR_9BIT;
    gagPrediction = gagPHT[index].isTaken();
    if (gagPrediction != taken)
        misPrediction[branchType].D_GAg++;
    if (taken)
        gagPHT[index].increment();
    else
        gagPHT[index].decrement();

    // E. gshare
    index = GHR_9BIT ^ ((UINT64)iaddr % GSHARE_PHT_SIZE);
    gsharePrediction = gsharePHT[index].isTaken();
    if (gsharePrediction != taken)
        misPrediction[branchType].E_gshare++;
    if (taken)
        gsharePHT[index].increment();
    else
        gsharePHT[index].decrement();

    // F. Hybrid of SAg and GAg
    index = GHR_9BIT;
    if (sagGagMetaPred[index].isTaken())
    {
        sagGagHybridPrediction = sagPrediction;
        sagGagGshareMetaPredEntry = &sagGshareMetaPred[index];
    }
    else
    {
        sagGagHybridPrediction = gagPrediction;
        sagGagGshareMetaPredEntry = &gagGshareMetaPred[index];
    }
    if (sagGagHybridPrediction != taken)
        misPrediction[branchType].F_SAg_GAg++;

    if (sagPrediction != gagPrediction)
    {
        if (sagPrediction == taken)
            sagGagMetaPred[index].increment();
        else
            sagGagMetaPred[index].decrement();
    }

    // G. Hybrid of SAg, GAg, and gshare
    // Majority of threee
    hybridPrediction = (sagPrediction + gagPrediction + gsharePrediction) >= 2;
    if (hybridPrediction != taken)
        misPrediction[branchType].G1_SAg_GAg_gshare_majority++;

    // Tournament
    if (sagGagGshareMetaPredEntry->isTaken())
        hybridPrediction = sagGagHybridPrediction;
    else
        hybridPrediction = gsharePrediction;
    if (sagGagHybridPrediction != gsharePrediction)
    {
        if (sagGagHybridPrediction == taken)
            sagGagGshareMetaPredEntry->increment();
        else
            sagGagGshareMetaPredEntry->decrement();
    }

    if (hybridPrediction != taken)
        misPrediction[branchType].G2_SAg_GAg_gshare_tournament++;

    GHR_9BIT = ((GHR_9BIT << 1) | taken) & GHR_MASK;
}

VOID AnalyzeIndirectControlFlow(ADDRINT iaddr, ADDRINT target)
{
    btbLookupCount++;

    // 1. Branch target buffer (BTB) indexed with PC
    UINT64 index = (UINT64)iaddr % BTB_SETS;
    BTBEntry *entry = btbPC[index].find((UINT64)iaddr);
    if (entry == NULL || entry->target != (UINT64)target)
    {
        btbPCMissCount++;
        btbPC[index].insert((UINT64)iaddr, (UINT64)target);
    }

    // 2. Branch target buffer (BTB) indexed with PC and GHR
    index = ((UINT64)iaddr % BTB_SETS) ^ (GHR_9BIT & BTB_PC_GHR_MASK);
    entry = btbPCGHR[index].find((UINT64)iaddr);
    if (entry == NULL || entry->target != (UINT64)target)
    {
        btbPCGHRTotalCount++;
        btbPCGHR[index].insert((UINT64)iaddr, (UINT64)target);
    }
}

VOID DoInsCount(UINT32 bblInsCount)
{
    insCount += bblInsCount;
}

ADDRINT CheckFastForward(void)
{
    return (insCount >= fastForward);
}

ADDRINT CheckTerminate(void)
{
    return (insCount >= fastForward + BILLION);
}

VOID Terminate(void)
{
    PIN_ExitApplication(0);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        // loop over all instructions in the basic block
        INS ins = BBL_InsTail(bbl);

        if (INS_Category(ins) == XED_CATEGORY_COND_BR)
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeUncondBranch, IARG_INST_PTR,
                IARG_BRANCH_TAKEN, IARG_BRANCH_TARGET_ADDR, IARG_END);
        }

        if (INS_IsIndirectControlFlow(ins))
        {
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeIndirectControlFlow, IARG_INST_PTR,
                IARG_BRANCH_TARGET_ADDR, IARG_END);
        }

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    if (code != 0)
    {
        *out << "=====================================================================" << endl;
        *out << "This application is terminated by PIN." << endl;
        *out << "=====================================================================" << endl;
        return;
    }

    *out << "=====================================================================" << endl;
    *out << "HW2 analysis results from " << KnobOutputFile.Value() << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Fast forward at:        " << fastForward << endl;
    *out << "Number of instructions after fast forward: " << insCount - fastForward << endl;

    *out << "=====================================================================" << endl;
    // PART A: DIRECTION PREDICTORS FOR CONDITIONAL BRANCHES
    *out << "PART A: DIRECTION PREDICTORS FOR CONDITIONAL BRANCHES" << endl;
    *out << endl;
    *out << "Forward Branch Mis-prediction counts:" << endl;
    PrintMisprictionMetrics(misPrediction[ForwardBranch]);
    *out << "Backward Branch Mis-prediction counts:" << endl;
    PrintMisprictionMetrics(misPrediction[BackwardBranch]);
    *out << "Overall Mis-prediction counts:" << endl;
    PrintMisprictionMetrics(misPrediction[ForwardBranch] + misPrediction[BackwardBranch]);
    *out << "=====================================================================" << endl;
    // PART B: TARGET PREDICTORS FOR INDIRECT CONTROL FLOW INSTRUCTIONS
    *out << "PART B: TARGET PREDICTORS FOR INDIRECT CONTROL FLOW INSTRUCTIONS" << endl;
    *out << "BTB lookup count:      " << setw(9) << right << btbLookupCount << endl;
    *out << "BTB PC miss count:     " << METRICS_FMT(btbPCMissCount, btbLookupCount);
    *out << "BTB PC+GHR miss count: " << METRICS_FMT(btbPCGHRTotalCount, btbLookupCount);
    *out << "\n=====================================================================" << endl;

    std::chrono::time_point<std::chrono::system_clock> endTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = endTime - startTime;
    *out << "\nTime elapsed: " << elapsed_seconds.count() / 60 << " minutes" << endl;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    startTime = std::chrono::system_clock::now();
    fastForward = KnobFastForward.Value() * BILLION;
    string fileName = KnobOutputFile.Value();

    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
