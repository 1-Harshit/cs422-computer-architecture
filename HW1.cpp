/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::endl;
using std::string;

typedef struct _InstMetrics
{
    UINT64 numInst = 0;
    UINT64 numLoads = 0;
    UINT64 numStores = 0;
    UINT64 numNops = 0;
    UINT64 numDirectCalls = 0;
    UINT64 numIndirectCalls = 0;
    UINT64 numReturns = 0;
    UINT64 numUncondBranches = 0;
    UINT64 numCondBranches = 0;
    UINT64 numLogicalOps = 0;
    UINT64 numRotateShift = 0;
    UINT64 numFlagOps = 0;
    UINT64 numVector = 0;
    UINT64 numCondMoves = 0;
    UINT64 numMMXSSE = 0;
    UINT64 numSysCalls = 0;
    UINT64 numFP = 0;
    UINT64 numRest = 0;
} InstMetrics;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 bblCount = 0; // number of dynamically executed basic blocks
UINT64 fastForward = 0;
InstMetrics *instMetrics = 0;

std::ostream *out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                             "fast forward this many billion instructions before starting to collect data");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
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

/*!
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   bblInstCount Instruction of various instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID AnalyseBblMetrics(InstMetrics *bblInstCount)
{
    bblCount++;
    instMetrics->numInst += bblInstCount->numInst;
    instMetrics->numLoads += bblInstCount->numLoads;
    instMetrics->numStores += bblInstCount->numStores;
    instMetrics->numNops += bblInstCount->numNops;
    instMetrics->numDirectCalls += bblInstCount->numDirectCalls;
    instMetrics->numIndirectCalls += bblInstCount->numIndirectCalls;
    instMetrics->numReturns += bblInstCount->numReturns;
    instMetrics->numUncondBranches += bblInstCount->numUncondBranches;
    instMetrics->numCondBranches += bblInstCount->numCondBranches;
    instMetrics->numLogicalOps += bblInstCount->numLogicalOps;
    instMetrics->numRotateShift += bblInstCount->numRotateShift;
    instMetrics->numFlagOps += bblInstCount->numFlagOps;
    instMetrics->numVector += bblInstCount->numVector;
    instMetrics->numCondMoves += bblInstCount->numCondMoves;
    instMetrics->numMMXSSE += bblInstCount->numMMXSSE;
    instMetrics->numSysCalls += bblInstCount->numSysCalls;
    instMetrics->numFP += bblInstCount->numFP;
    instMetrics->numRest += bblInstCount->numRest;
}

VOID DoInsCount(UINT64 bblInsCount)
{
    insCount += bblInsCount;
}

ADDRINT CheckFastForward(void)
{
    return (insCount >= fastForward);
}

ADDRINT CheckTerminate(void)
{
    return (insCount >= fastForward + 1e9);
}

VOID Terminate(void)
{
    PIN_ExitApplication(0);
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Insert call to the AnalyseBblMetrics() analysis routine before every basic block
 * of the trace.
 * This function is called every time a new trace is encountered.
 * @param[in]   trace    trace to be instrumented
 * @param[in]   v        value specified by the tool in the TRACE_AddInstrumentFunction
 *                       function call
 */
VOID Trace(TRACE trace, VOID *v)
{
    // Visit every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        InstMetrics *bblInstCount = new InstMetrics();

        bblInstCount->numInst = BBL_NumIns(bbl);

        // loop over all instructions in the basic block
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_Category(ins) == XED_CATEGORY_INVALID)
            {
                continue;
            }
            // if memory read instruction
            if (INS_IsMemoryRead(ins))
            {
                bblInstCount->numLoads++;
            }
            else if (INS_IsMemoryWrite(ins))
            {
                bblInstCount->numStores++;
                continue;
            }
            switch (INS_Category(ins))
            {
            case XED_CATEGORY_NOP:
                bblInstCount->numNops++;
                break;
            case XED_CATEGORY_CALL:
                if (INS_IsDirectCall(ins))
                    bblInstCount->numDirectCalls++;
                else
                    bblInstCount->numIndirectCalls++;
                break;
            case XED_CATEGORY_RET:
                bblInstCount->numReturns++;
                break;
            case XED_CATEGORY_UNCOND_BR:
                bblInstCount->numUncondBranches++;
                break;
            case XED_CATEGORY_COND_BR:
                bblInstCount->numCondBranches++;
                break;
            case XED_CATEGORY_LOGICAL:
                bblInstCount->numLogicalOps++;
                break;
            case XED_CATEGORY_ROTATE:
            case XED_CATEGORY_SHIFT:
                bblInstCount->numRotateShift++;
                break;
            case XED_CATEGORY_FLAGOP:
                bblInstCount->numFlagOps++;
                break;
            case XED_CATEGORY_AVX:
            case XED_CATEGORY_AVX2:
            case XED_CATEGORY_AVX2GATHER:
            case XED_CATEGORY_AVX512:
                bblInstCount->numVector++;
                break;
            case XED_CATEGORY_CMOV:
                bblInstCount->numCondMoves++;
                break;
            case XED_CATEGORY_MMX:
            case XED_CATEGORY_SSE:
                bblInstCount->numMMXSSE++;
                break;
            case XED_CATEGORY_SYSCALL:
                bblInstCount->numSysCalls++;
                break;
            case XED_CATEGORY_X87_ALU:
                bblInstCount->numFP++;
                break;
            default:
                bblInstCount->numRest++;
                break;
            }
        }

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT64, bblInstCount->numInst, IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)AnalyseBblMetrics, IARG_PTR, bblInstCount, IARG_END);
    }
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
    *out << "===============================================" << endl;
    *out << "HW1 analysis results: " << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Fast forward at: " << fastForward << endl;
    *out << "Number of basic blocks: " << bblCount << endl;
    *out << "Number of instructions: " << instMetrics->numInst << endl;
    *out << "Number of loads: " << instMetrics->numLoads << endl;
    *out << "Number of stores: " << instMetrics->numStores << endl;
    *out << "Number of nops: " << instMetrics->numNops << endl;
    *out << "Number of direct calls: " << instMetrics->numDirectCalls << endl;
    *out << "Number of indirect calls: " << instMetrics->numIndirectCalls << endl;
    *out << "Number of returns: " << instMetrics->numReturns << endl;
    *out << "Number of unconditional branches: " << instMetrics->numUncondBranches << endl;
    *out << "Number of conditional branches: " << instMetrics->numCondBranches << endl;
    *out << "Number of logical operations: " << instMetrics->numLogicalOps << endl;
    *out << "Number of rotate/shift operations: " << instMetrics->numRotateShift << endl;
    *out << "Number of flag operations: " << instMetrics->numFlagOps << endl;
    *out << "Number of vector operations: " << instMetrics->numVector << endl;
    *out << "Number of conditional moves: " << instMetrics->numCondMoves << endl;
    *out << "Number of MMX/SSE operations: " << instMetrics->numMMXSSE << endl;
    *out << "Number of system calls: " << instMetrics->numSysCalls << endl;
    *out << "Number of floating point operations: " << instMetrics->numFP << endl;
    *out << "Number of other instructions: " << instMetrics->numRest << endl;
    *out << "===============================================" << endl;
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

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    instMetrics = new InstMetrics();
    fastForward = KnobFastForward.Value() * 1e9;

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

