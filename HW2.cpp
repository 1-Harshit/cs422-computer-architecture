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
using std::string;

#define BILLION 1000000000

/* ================================================================== */
// Custom Structures
/* ================================================================== */

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

/* ================================================================== */
// Global variables
/* ================================================================== */
// UTILS
UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 fastForward = 0;
std::ostream *out = &cerr;
std::chrono::time_point<std::chrono::system_clock> startTime;

// PART A
/*
 * Implement the following conditional branch predictors and count the number of mispredictions.
 * A. Static forward not-taken and backward taken (FNBT)
 * B. Bimodal
 * C. SAg
 * D. GAg
 * E. gshare
 * F. Hybrid of SAg and GAg
 * G. Hybrid of SAg, GAg, and gshare */
MisPrediction misPrediction;

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

INT32 Usage()
{
    cerr << "This tool prints out the number of dynamically executed " << endl
         << "instructions, basic blocks and threads in the application." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

#define FNBT_PREDICTION(target, iaddr) (target < iaddr)

VOID AnalyzeUncondBranch(VOID *iaddr, BOOL taken, VOID *target)
{
    misPrediction.Count++;
    UINT64 index;
    BOOL sagPrediction, gagPrediction, gsharePrediction, sagGagHybridPrediction, hybridPrediction;
    SaturatingCounter<HYBRID_MP_BITS> *sagGagGshareMetaPredEntry;

    // A. Static FNBT
    if (FNBT_PREDICTION(target, iaddr) != taken)
        misPrediction.A_static_FNBT++;

    // B. Bimodal predictor: 512x2-bit PHT
    index = (UINT64)iaddr % BIMODAL_PHT_SIZE;
    if (bimodalPHT[index].isTaken() != taken)
        misPrediction.B_bimodal++;
    if (taken)
        bimodalPHT[index].increment();
    else
        bimodalPHT[index].decrement();

    // C. SAg
    index = (UINT64)iaddr % SAG_BHT_SIZE;
    sagPrediction = sagPHT[sagBHT[index]].isTaken();
    if (sagPrediction != taken)
        misPrediction.C_SAg++;
    if (taken)
        sagPHT[sagBHT[index]].increment();
    else
        sagPHT[sagBHT[index]].decrement();
    sagBHT[index] = ((sagBHT[index] << 1) | taken) & SAG_BHT_MASK;

    // D. GAg
    index = GHR_9BIT;
    gagPrediction = gagPHT[index].isTaken();
    if (gagPrediction != taken)
        misPrediction.D_GAg++;
    if (taken)
        gagPHT[index].increment();
    else
        gagPHT[index].decrement();

    // E. gshare
    index = GHR_9BIT ^ ((UINT64)iaddr % GSHARE_PHT_SIZE);
    gsharePrediction = gsharePHT[index].isTaken();
    if (gsharePrediction != taken)
        misPrediction.E_gshare++;
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
        misPrediction.F_SAg_GAg++;

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
        misPrediction.G1_SAg_GAg_gshare_majority++;

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
        misPrediction.G2_SAg_GAg_gshare_tournament++;

    GHR_9BIT = ((GHR_9BIT << 1) | taken) & GHR_MASK;
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

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    if (code != 0)
    {
        *out << "===============================================" << endl;
        *out << "This application is terminated by PIN." << endl;
        *out << "===============================================" << endl;
        return;
    }

    *out << "===============================================" << endl;
    *out << "HW2 analysis results from " << KnobOutputFile.Value() << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Fast forward at:        " << fastForward << endl;
    *out << "Number of instructions after fast forward: " << insCount - fastForward << endl;

    *out << "===============================================" << endl;
    // PART A: DIRECTION PREDICTORS FOR CONDITIONAL BRANCHES
    *out << "PART A: DIRECTION PREDICTORS FOR CONDITIONAL BRANCHES" << endl;
    *out << "Mis-prediction counts:" << endl;
    *out << "Total number of branches: " << misPrediction.Count << endl;
    *out << "A. Static FNBT: " << misPrediction.A_static_FNBT << endl;
    *out << "B. Bimodal predictor: " << misPrediction.B_bimodal << endl;
    *out << "C. SAg: " << misPrediction.C_SAg << endl;
    *out << "D. GAg: " << misPrediction.D_GAg << endl;
    *out << "E. gshare: " << misPrediction.E_gshare << endl;
    *out << "F. Hybrid of SAg and GAg: " << misPrediction.F_SAg_GAg << endl;
    *out << "G. Hybrid of SAg, GAg, and gshare (majority): " << misPrediction.G1_SAg_GAg_gshare_majority << endl;
    *out << "G. Hybrid of SAg, GAg, and gshare (tournament): " << misPrediction.G2_SAg_GAg_gshare_tournament << endl;

    *out << "\n===============================================" << endl;
    // PART B: TARGET PREDICTORS FOR INDIRECT CONTROL FLOW INSTRUCTIONS
    *out << "PART B: TARGET PREDICTORS FOR INDIRECT CONTROL FLOW INSTRUCTIONS" << endl;

    *out << "===============================================" << endl;

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
