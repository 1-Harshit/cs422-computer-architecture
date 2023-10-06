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

/* ================================================================== */
// Global variables
/* ================================================================== */
// UTILS
UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 fastForward = 0;
std::ostream *out = &cerr;
std::chrono::time_point<std::chrono::system_clock> startTime;
// PART A+B

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
        if (INS_Category(ins) == XED_CATEGORY_INVALID)
            continue;

        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);

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
