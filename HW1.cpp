/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_set>

using std::cerr;
using std::endl;
using std::string;
using std::unordered_set;

UINT32 granularity = 4; // bytes

typedef struct _InstMetrics
{
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
UINT64 fastForward = 0;
InstMetrics *instMetrics = 0;

unordered_set<UINT64> dataFootprint;
unordered_set<UINT64> insFootprint;
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
/*
VOID PredicatedAnalysisMetrics1Mem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, void *memOpAddr1, UINT32 memOpSize1)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
    dataFootprint.push_back({memOpAddr1, memOpSize1});
}

VOID PredicatedAnalysisMetrics2Mem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
    dataFootprint.push_back({memOpAddr1, memOpSize1});
    dataFootprint.push_back({memOpAddr2, memOpSize2});
}

VOID PredicatedAnalysisMetrics3Mem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
    dataFootprint.push_back({memOpAddr1, memOpSize1});
    dataFootprint.push_back({memOpAddr2, memOpSize2});
    dataFootprint.push_back({memOpAddr3, memOpSize3});
}

VOID PredicatedAnalysisMetrics4Mem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3, void *memOpAddr4, UINT32 memOpSize4)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
    dataFootprint.push_back({memOpAddr1, memOpSize1});
    dataFootprint.push_back({memOpAddr2, memOpSize2});
    dataFootprint.push_back({memOpAddr3, memOpSize3});
    dataFootprint.push_back({memOpAddr4, memOpSize4});
}

VOID PredicatedAnalysisMetrics5Mem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3, void *memOpAddr4, UINT32 memOpSize4, void *memOpAddr5, UINT32 memOpSize5)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
    dataFootprint.push_back({memOpAddr1, memOpSize1});
    dataFootprint.push_back({memOpAddr2, memOpSize2});
    dataFootprint.push_back({memOpAddr3, memOpSize3});
    dataFootprint.push_back({memOpAddr4, memOpSize4});
    dataFootprint.push_back({memOpAddr5, memOpSize5});
}
*/

VOID PredicatedAnalysisMetricsMem(UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores)
{
    *insTypeAddr += 1;
    instMetrics->numLoads += numLoads;
    instMetrics->numStores += numStores;
}

VOID PredicatedAnalysisMetrics(UINT64 *insTypeAddr)
{
    *insTypeAddr += 1;
}

VOID AnalysisMetrics(void *insAddr, UINT32 insSize)
{
    // assume data access to be in 32 bit chunks and 32 bit alinged
    // add all addresses in the range of [addr, addr + size) to data footprint
    // such that for 35-70 {32,64} is added
    for (UINT64 addr = (UINT64)insAddr / 32; addr < ((UINT64)insAddr + insSize) / 32 + (((UINT64)insAddr + insSize) % 32 != 0); addr++)
    {
        insFootprint.insert(addr);
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
    return (insCount >= fastForward + 1e9);
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
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_Category(ins) == XED_CATEGORY_INVALID)
                continue;

            // type A pointer to increment in instruction metrics stucture
            void *instypeaddr = 0;
            // type B: number of loads
            UINT32 numLoads = 0;
            // type B: number of stores
            UINT32 numStores = 0;
            // data size
            UINT32 dataSize = 0;
            // get size of memory read count of mem operands
            UINT32 memOperands = INS_MemoryOperandCount(ins);
            UINT32 readOperands = 0;
            UINT32 writeOperands = 0;

            for (UINT32 memOp = 0; memOp < memOperands; memOp++)
            {
                dataSize = INS_MemoryOperandSize(ins, memOp);
                // displacement = INS_MemoryOperandDisplacement(ins, memOp);
                if (INS_MemoryOperandIsRead(ins, memOp))
                {
                    numLoads += dataSize / granularity + (dataSize % granularity != 0);
                    readOperands++;
                }
                if (INS_MemoryOperandIsWritten(ins, memOp))
                {
                    numStores += dataSize / granularity + (dataSize % granularity != 0);
                    writeOperands++;
                }
            }

            switch (INS_Category(ins))
            {
            case XED_CATEGORY_NOP:
                instypeaddr = &instMetrics->numNops;
                break;
            case XED_CATEGORY_CALL:
                if (INS_IsDirectCall(ins))
                    instypeaddr = &instMetrics->numDirectCalls;
                else
                    instypeaddr = &instMetrics->numIndirectCalls;
                break;
            case XED_CATEGORY_RET:
                instypeaddr = &instMetrics->numReturns;
                break;
            case XED_CATEGORY_UNCOND_BR:
                instypeaddr = &instMetrics->numUncondBranches;
                break;
            case XED_CATEGORY_COND_BR:
                instypeaddr = &instMetrics->numCondBranches;
                break;
            case XED_CATEGORY_LOGICAL:
                instypeaddr = &instMetrics->numLogicalOps;
                break;
            case XED_CATEGORY_ROTATE:
            case XED_CATEGORY_SHIFT:
                instypeaddr = &instMetrics->numRotateShift;
                break;
            case XED_CATEGORY_FLAGOP:
                instypeaddr = &instMetrics->numFlagOps;
                break;
            case XED_CATEGORY_AVX:
            case XED_CATEGORY_AVX2:
            case XED_CATEGORY_AVX2GATHER:
            case XED_CATEGORY_AVX512:
                instypeaddr = &instMetrics->numVector;
                break;
            case XED_CATEGORY_CMOV:
                instypeaddr = &instMetrics->numCondMoves;
                break;
            case XED_CATEGORY_MMX:
            case XED_CATEGORY_SSE:
                instypeaddr = &instMetrics->numMMXSSE;
                break;
            case XED_CATEGORY_SYSCALL:
                instypeaddr = &instMetrics->numSysCalls;
                break;
            case XED_CATEGORY_X87_ALU:
                instypeaddr = &instMetrics->numFP;
                break;
            default:
                instypeaddr = &instMetrics->numRest;
                break;
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            if (numLoads || numStores)
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetricsMem, IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores, IARG_END);
            else
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics, IARG_PTR, instypeaddr, IARG_END);

            // INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            // switch (memOperands)
            // {
            // case 0:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics,
            //         IARG_PTR, instypeaddr, IARG_END);
            // case 1:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics1Mem,
            //         IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores,
            //         IARG_MEMORYOP_EA, 0, IARG_UINT32, INS_MemoryOperandSize(ins, 0), IARG_END);
            //     break;
            // case 2:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics2Mem,
            //         IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores,
            //         IARG_MEMORYOP_EA, 0, IARG_UINT32, INS_MemoryOperandSize(ins, 0),
            //         IARG_MEMORYOP_EA, 1, IARG_UINT32, INS_MemoryOperandSize(ins, 1), IARG_END);
            //     break;
            // case 3:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics3Mem,
            //         IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores,
            //         IARG_MEMORYOP_EA, 0, IARG_UINT32, INS_MemoryOperandSize(ins, 0),
            //         IARG_MEMORYOP_EA, 1, IARG_UINT32, INS_MemoryOperandSize(ins, 1),
            //         IARG_MEMORYOP_EA, 2, IARG_UINT32, INS_MemoryOperandSize(ins, 2), IARG_END);
            //     break;
            // case 4:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics4Mem,
            //         IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores,
            //         IARG_MEMORYOP_EA, 0, IARG_UINT32, INS_MemoryOperandSize(ins, 0),
            //         IARG_MEMORYOP_EA, 1, IARG_UINT32, INS_MemoryOperandSize(ins, 1),
            //         IARG_MEMORYOP_EA, 2, IARG_UINT32, INS_MemoryOperandSize(ins, 2),
            //         IARG_MEMORYOP_EA, 3, IARG_UINT32, INS_MemoryOperandSize(ins, 3), IARG_END);
            //     break;
            // case 5:
            //     INS_InsertThenPredicatedCall(
            //         ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics5Mem,
            //         IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores,
            //         IARG_MEMORYOP_EA, 0, IARG_UINT32, INS_MemoryOperandSize(ins, 0),
            //         IARG_MEMORYOP_EA, 1, IARG_UINT32, INS_MemoryOperandSize(ins, 1),
            //         IARG_MEMORYOP_EA, 2, IARG_UINT32, INS_MemoryOperandSize(ins, 2),
            //         IARG_MEMORYOP_EA, 3, IARG_UINT32, INS_MemoryOperandSize(ins, 3),
            //         IARG_MEMORYOP_EA, 4, IARG_UINT32, INS_MemoryOperandSize(ins, 4), IARG_END);
            //     break;
            // default:
            //     *out << "ERROR: more than 5 memory operands" << endl;
            //     *out << "ERROR: " << INS_Disassemble(ins) << endl;
            //     break;
            // }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)AnalysisMetrics,
                IARG_INST_PTR, IARG_UINT32, INS_Size(ins), IARG_END);
        }
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

#define PRINT_METRICS(name, total) name << " (" << 100.0 * name / total << "%)" << endl

VOID Fini(INT32 code, VOID *v)
{
    if (code != 0)
    {
        *out << "===============================================" << endl;
        *out << "This application is terminated by PIN." << endl;
        *out << "===============================================" << endl;
        return;
    }
    UINT64 total = 0;
    total += instMetrics->numLoads;
    total += instMetrics->numStores;
    total += instMetrics->numNops;
    total += instMetrics->numDirectCalls;
    total += instMetrics->numIndirectCalls;
    total += instMetrics->numReturns;
    total += instMetrics->numUncondBranches;
    total += instMetrics->numCondBranches;
    total += instMetrics->numLogicalOps;
    total += instMetrics->numRotateShift;
    total += instMetrics->numFlagOps;
    total += instMetrics->numVector;
    total += instMetrics->numCondMoves;
    total += instMetrics->numMMXSSE;
    total += instMetrics->numSysCalls;
    total += instMetrics->numFP;
    total += instMetrics->numRest;

    *out << "===============================================" << endl;
    *out << "HW1 analysis results: " << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Fast forward at: " << fastForward << endl;
    *out << "Number of instructions after fast forward: " << insCount - fastForward << endl;
    *out << "=====================PARTA=====================" << endl;
    *out << "Number of loads: " << PRINT_METRICS(instMetrics->numLoads, total);
    *out << "Number of stores: " << PRINT_METRICS(instMetrics->numStores, total);
    *out << "Number of nops: " << PRINT_METRICS(instMetrics->numNops, total);
    *out << "Number of direct calls: " << PRINT_METRICS(instMetrics->numDirectCalls, total);
    *out << "Number of indirect calls: " << PRINT_METRICS(instMetrics->numIndirectCalls, total);
    *out << "Number of returns: " << PRINT_METRICS(instMetrics->numReturns, total);
    *out << "Number of unconditional branches: " << PRINT_METRICS(instMetrics->numUncondBranches, total);
    *out << "Number of conditional branches: " << PRINT_METRICS(instMetrics->numCondBranches, total);
    *out << "Number of logical operations: " << PRINT_METRICS(instMetrics->numLogicalOps, total);
    *out << "Number of rotate/shift operations: " << PRINT_METRICS(instMetrics->numRotateShift, total);
    *out << "Number of flag operations: " << PRINT_METRICS(instMetrics->numFlagOps, total);
    *out << "Number of vector operations: " << PRINT_METRICS(instMetrics->numVector, total);
    *out << "Number of conditional moves: " << PRINT_METRICS(instMetrics->numCondMoves, total);
    *out << "Number of MMX/SSE operations: " << PRINT_METRICS(instMetrics->numMMXSSE, total);
    *out << "Number of system calls: " << PRINT_METRICS(instMetrics->numSysCalls, total);
    *out << "Number of floating point operations: " << PRINT_METRICS(instMetrics->numFP, total);
    *out << "Number of other instructions: " << PRINT_METRICS(instMetrics->numRest, total);
    *out << "=====================PARTB=====================" << endl;
    // CPI numloads You should charge each load and store operation a fixed latency of
    // seventy cycles and every other instruction a latency of one cycle.
    FLT64 cpi = (instMetrics->numLoads + instMetrics->numStores) * 70.0 / total + (total - instMetrics->numLoads - instMetrics->numStores) * 1.0 / total;
    *out << "CPI: " << cpi << endl;
    *out << "=====================PARTC=====================" << endl;
    // Instruction footprint
    *out << "Number of 32 bytes block for instructions " << insFootprint.size() << endl;
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
