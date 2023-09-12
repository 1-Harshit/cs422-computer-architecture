/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

/*! @file
 *  This is an example of the PIN tool that demonstrates some basic PIN APIs
 *  and could serve as the starting point for developing your first PIN tool
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::endl;
using std::string;

typedef struct _InstCount
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
} InstCount;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 bblCount = 0; // number of dynamically executed basic blocks
UINT64 fastForward = 0;
InstCount *instCount = 0;

std::ostream *out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for MyPinTool output");

KNOB<BOOL> KnobCount(KNOB_MODE_WRITEONCE, "pintool", "count", "1",
                     "count instructions, basic blocks and threads in the application");

KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0",
                             "fast forward this billion many instructions before starting to collect data");

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
 * Increase counter of the executed basic blocks and instructions.
 * This function is called for every basic block when it is about to be executed.
 * @param[in]   bblInstCount Instruction of various instructions in the basic block
 * @note use atomic operations for multi-threaded applications
 */
VOID CountBbl(InstCount *bblInstCount)
{
    bblCount++;
    instCount->numInst += bblInstCount->numInst;
    instCount->numLoads += bblInstCount->numLoads;
    instCount->numStores += bblInstCount->numStores;
    instCount->numNops += bblInstCount->numNops;
    instCount->numDirectCalls += bblInstCount->numDirectCalls;
    instCount->numIndirectCalls += bblInstCount->numIndirectCalls;
    instCount->numReturns += bblInstCount->numReturns;
    instCount->numUncondBranches += bblInstCount->numUncondBranches;
    instCount->numCondBranches += bblInstCount->numCondBranches;
    instCount->numLogicalOps += bblInstCount->numLogicalOps;
    instCount->numRotateShift += bblInstCount->numRotateShift;
    instCount->numFlagOps += bblInstCount->numFlagOps;
    instCount->numVector += bblInstCount->numVector;
    instCount->numCondMoves += bblInstCount->numCondMoves;
    instCount->numMMXSSE += bblInstCount->numMMXSSE;
    instCount->numSysCalls += bblInstCount->numSysCalls;
    instCount->numFP += bblInstCount->numFP;
    instCount->numRest += bblInstCount->numRest;
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
 * Insert call to the CountBbl() analysis routine before every basic block
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
        InstCount *bblInstCount = new InstCount();

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

        // Insert a call to CountBbl() before every basic bloc, passing the pointer to instcount
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT64, bblInstCount->numInst, IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)CountBbl, IARG_PTR, bblInstCount, IARG_END);
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
    *out << "Number of instructions: " << instCount->numInst << endl;
    *out << "Number of loads: " << instCount->numLoads << endl;
    *out << "Number of stores: " << instCount->numStores << endl;
    *out << "Number of nops: " << instCount->numNops << endl;
    *out << "Number of direct calls: " << instCount->numDirectCalls << endl;
    *out << "Number of indirect calls: " << instCount->numIndirectCalls << endl;
    *out << "Number of returns: " << instCount->numReturns << endl;
    *out << "Number of unconditional branches: " << instCount->numUncondBranches << endl;
    *out << "Number of conditional branches: " << instCount->numCondBranches << endl;
    *out << "Number of logical operations: " << instCount->numLogicalOps << endl;
    *out << "Number of rotate/shift operations: " << instCount->numRotateShift << endl;
    *out << "Number of flag operations: " << instCount->numFlagOps << endl;
    *out << "Number of vector operations: " << instCount->numVector << endl;
    *out << "Number of conditional moves: " << instCount->numCondMoves << endl;
    *out << "Number of MMX/SSE operations: " << instCount->numMMXSSE << endl;
    *out << "Number of system calls: " << instCount->numSysCalls << endl;
    *out << "Number of floating point operations: " << instCount->numFP << endl;
    *out << "Number of other instructions: " << instCount->numRest << endl;
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

    instCount = new InstCount();
    fastForward = 207 * 1e9;

    if (KnobCount)
    {
        // Register function to be called to instrument traces
        TRACE_AddInstrumentFunction(Trace, 0);

        // Register function to be called when the application exits
        PIN_AddFiniFunction(Fini, 0);
    }

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
