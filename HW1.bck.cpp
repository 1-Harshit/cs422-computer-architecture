/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <vector>
using std::cerr;
using std::endl;
using std::string;
using std::vector;

#define ANALYSIS_LEN UINT64(1e8)

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

typedef struct _Footprint
{
    void *addr;
    UINT32 size;
} Footprint;

typedef struct _DataMetrics
{
    UINT64 readOperands = 0;
    UINT64 writeOperands = 0;
    UINT64 memTouched = 0;
    UINT64 maxMemTouched = -1;
} DataMetrics;

/* ================================================================== */
// Global variables
/* ================================================================== */

UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 fastForward = 0;
InstMetrics *instMetrics = 0;
DataMetrics *dataMetrics = 0;
vector<Footprint> dataFootprint;
vector<Footprint> insFootprint;
vector<UINT32> numOperands, numWriteRegs, numReadRegs;
// INT32 minIntermideate = INT32_MAX, maxIntermideate = INT32_MIN;
// ADDRDELTA minDisplacement = INT32_MAX, maxDisplacement = INT32_MIN;

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

VOID IncrementInstMetrics(void *instypeaddr)
{
    UINT64 *instype = (UINT64 *)instypeaddr;
    *instype += 1;
}

VOID IncrementInstMetricsLoadStore(void *instypeaddr, UINT32 numStores, UINT32 numLoads, VOID *addr, UINT32 size, ADDRDELTA displacement)
{
    UINT64 *instype = (UINT64 *)instypeaddr;
    *instype += 1;
    instMetrics->numStores += numStores;
    instMetrics->numLoads += numLoads;
    dataFootprint.push_back({addr, size});
    dataMetrics->memTouched += size;
    dataMetrics->readOperands += numLoads > 0;
    dataMetrics->writeOperands += numStores > 0;
    // if (dataMetrics->maxMemTouched < size)
    //     dataMetrics->maxMemTouched = size;
    // if (minDisplacement > displacement)
    //     minDisplacement = displacement;
    // if (maxDisplacement < displacement)
    // maxDisplacement = displacement;
}

VOID RecordInstMetrics(VOID *insaddr, UINT32 size, UINT32 numOperand, UINT32 numWriteReg, UINT32 numReadReg, INT32 minimumIntermideate, INT32 maximumIntermideate)
{
    insFootprint.push_back({insaddr, size});
    numOperands.push_back(numOperand);
    numWriteRegs.push_back(numWriteReg);
    numReadRegs.push_back(numReadReg);
    // if (minIntermideate > minimumIntermideate)
    //     minIntermideate = minimumIntermideate;
    // if (maxIntermideate < maximumIntermideate)
    //     maxIntermideate = maximumIntermideate;
}

VOID IncrementInsCount(UINT64 bblInsCount)
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
            // instruction size
            UINT32 insSize = INS_Size(ins);
            
            // get size of memory read
            UINT32 memOperands = INS_MemoryOperandCount(ins);
            // displacement value
            // UINT32 displacement = 0;

            if (memOperands > 1)
            {
                *out << "WARNING: more than one memory operand: " << INS_Disassemble(ins) << endl;
                *out << "         This tool supports only one memory operand." << endl;
                PIN_ExitApplication(1);
            }

            for (UINT32 memOp = 0; memOp < memOperands; memOp++)
            {
                dataSize = INS_MemoryOperandSize(ins, memOp);
                // displacement = INS_MemoryOperandDisplacement(ins, memOp);
                if (INS_MemoryOperandIsRead(ins, memOp))
                {
                    numLoads = dataSize / granularity + (dataSize % granularity != 0);
                }
                if (INS_MemoryOperandIsWritten(ins, memOp))
                {
                    numStores = dataSize / granularity + (dataSize % granularity != 0);
                }
            }

            switch (INS_Category(ins))
            {
            case XED_CATEGORY_NOP:
                instypeaddr = &instMetrics->numNops;
                break;
            case XED_CATEGORY_CALL:
            {
                if (INS_IsDirectCall(ins))
                    instypeaddr = &instMetrics->numDirectCalls;
                else
                    instypeaddr = &instMetrics->numIndirectCalls;
                break;
            }
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

            UINT32 numOperand = INS_OperandCount(ins);
            INT32 minimumIntermideate = INT32_MAX; // Set initial minimum to maximum possible value
            INT32 maximumIntermideate = INT32_MIN; // Set initial maximum to minimum possible value

            for (UINT32 i = 0; i < numOperand; i++)
            {
                if (INS_OperandIsImmediate(ins, i))
                {
                    INT32 immediateValue = INS_OperandImmediate(ins, i);

                    // Update minimum and maximum values
                    if (immediateValue < minimumIntermideate)
                        minimumIntermideate = immediateValue;

                    if (immediateValue > maximumIntermideate)
                        maximumIntermideate = immediateValue;
                }
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            if (numLoads > 0 || numStores > 0)
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)IncrementInstMetricsLoadStore, IARG_PTR, instypeaddr,
                    IARG_UINT32, numLoads, IARG_UINT32, numStores, IARG_MEMORYOP_EA, 0, IARG_UINT32,
                    dataSize, IARG_ADDRINT, 0, IARG_END);
            else
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)IncrementInstMetrics, IARG_PTR, instypeaddr, IARG_END);

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)RecordInstMetrics, IARG_INST_PTR, IARG_UINT32,
                insSize, IARG_UINT32, numOperand, IARG_UINT32, INS_MaxNumRRegs(ins), IARG_UINT32,
                INS_MaxNumWRegs(ins), IARG_ADDRINT, minimumIntermideate, IARG_ADDRINT, maximumIntermideate, IARG_END);
        }

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)IncrementInsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

VOID AnalyseFootprint()
{
    // sort data footprint vector with respect to address if address same sort with respect to size
    std::sort(dataFootprint.begin(), dataFootprint.end(), [](const Footprint &a, const Footprint &b)
              { return a.addr < b.addr || (a.addr == b.addr && a.size < b.size); });
    // sort instruction footprint vector with respect to address
    std::sort(insFootprint.begin(), insFootprint.end(), [](const Footprint &a, const Footprint &b)
              { return a.addr < b.addr || (a.addr == b.addr && a.size < b.size); });

    // count number of unique 32 byte blocks in data footprint
    UINT64 dataFootprintBlocks = 0, insFootprintBlocks = 0;
    UINT64 lastAddr = 0;
    for (UINT64 i = 0; i < dataFootprint.size(); i++)
    {
        if (lastAddr < (UINT64)dataFootprint[i].addr)
            lastAddr = (UINT64)dataFootprint[i].addr - (UINT64)dataFootprint[i].addr % 32;
        while (lastAddr < (UINT64)dataFootprint[i].addr + dataFootprint[i].size)
        {
            lastAddr += 32;
            dataFootprintBlocks++;
        }
    }
    lastAddr = 0;
    for (UINT64 i = 0; i < insFootprint.size(); i++)
    {
        if (lastAddr < (UINT64)insFootprint[i].addr)
            lastAddr = (UINT64)insFootprint[i].addr - (UINT64)insFootprint[i].addr % 32;
        while (lastAddr < (UINT64)insFootprint[i].addr + insFootprint[i].size)
        {
            lastAddr += 32;
            insFootprintBlocks++;
        }
    }

    *out << "Number of unique 32 byte blocks in data footprint: " << dataFootprintBlocks << endl;
    *out << "Number of unique 32 byte blocks in instruction footprint: " << insFootprintBlocks << endl;
}

VOID AnalyseDistribution(UINT64 numIns)
{
    *out << "Distribution of instruction sizes: " << endl;
    // insfootprint size distribution
    std::sort(insFootprint.begin(), insFootprint.end(), [](const Footprint &a, const Footprint &b)
              { return a.size < b.size; });
    INT64 lastSize = -1;
    UINT64 count = 0;
    for (UINT64 i = 0; i < insFootprint.size(); i++)
    {
        if (lastSize != insFootprint[i].size)
        {
            if (lastSize != 0)
                *out << lastSize << " bytes: " << count << endl;
            lastSize = insFootprint[i].size;
            count = 1;
        }
        else
            count++;
    }
    // distribution of number of operands
    *out << "Distribution of number of operands: " << endl;
    std::sort(numOperands.begin(), numOperands.end());
    lastSize = -1;
    count = 0;
    for (UINT64 i = 0; i < numOperands.size(); i++)
    {
        if (lastSize != numOperands[i])
        {
            if (lastSize != -1)
                *out << lastSize << " operands: " << count << endl;
            lastSize = numOperands[i];
            count = 1;
        }
        else
            count++;
    }
    // distribution of number of read registers
    *out << "Distribution of number of read registers: " << endl;
    std::sort(numReadRegs.begin(), numReadRegs.end());
    lastSize = -1;
    count = 0;
    for (UINT64 i = 0; i < numReadRegs.size(); i++)
    {
        if (lastSize != numReadRegs[i])
        {
            if (lastSize != -1)
                *out << lastSize << " read registers: " << count << endl;
            lastSize = numReadRegs[i];
            count = 1;
        }
        else
            count++;
    }
    // distribution of number of write registers
    *out << "Distribution of number of write registers: " << endl;
    std::sort(numWriteRegs.begin(), numWriteRegs.end());
    lastSize = -1;
    count = 0;
    for (UINT64 i = 0; i < numWriteRegs.size(); i++)
    {
        if (lastSize != numWriteRegs[i])
        {
            if (lastSize != -1)
                *out << lastSize << " write registers: " << count << endl;
            lastSize = numWriteRegs[i];
            count = 1;
        }
        else
            count++;
    }
    // distribution of number of read operands
    *out << "Distribution of number of read operands: " << endl;
    *out << "0 read operands: " << numIns - dataMetrics->readOperands << endl;
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
    FLT64 cpi = (instMetrics->numLoads + instMetrics->numStores) * 70 / total + (total - instMetrics->numLoads - instMetrics->numStores) * 1 / total;
    *out << "CPI: " << cpi << endl;
    *out << "=====================PARTC=====================" << endl;
    AnalyseFootprint();
    *out << "=====================PARTD=====================" << endl;
    AnalyseDistribution(total - instMetrics->numLoads - instMetrics->numStores);
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

    dataFootprint.reserve(ANALYSIS_LEN);
    insFootprint.reserve(ANALYSIS_LEN);

    instMetrics = new InstMetrics();
    dataMetrics = new DataMetrics();
    fastForward = KnobFastForward.Value() * 1e9;

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    *out << "===============================================" << endl;
    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}