/*
 * Copyright (C) 2007-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <map>
#include <chrono>

using std::cerr;
using std::endl;
using std::map;
using std::string;
using std::unordered_set;

UINT32 granularity = 4; // bytes

/* ================================================================== */
// Custom Structures
/* ================================================================== */
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
// UTILS
UINT64 insCount = 0; // number of dynamically executed instructions
UINT64 fastForward = 0;
std::ostream *out = &cerr;
std::chrono::time_point<std::chrono::system_clock> startTime;
// PART A+B
InstMetrics *instMetrics = 0;
// PART C
unordered_set<UINT64> dataFootprint;
unordered_set<UINT64> insFootprint;
// PART D
map<UINT32, UINT64> insLengthMap;
map<UINT32, UINT64> insOperandsMap;
map<UINT32, UINT64> insRegReadMap;
map<UINT32, UINT64> insRegWriteMap;
map<UINT32, UINT64> insMemOperandsMap;
map<UINT32, UINT64> insMemReadMap;
map<UINT32, UINT64> insMemWriteMap;
UINT64 insMemTouched = 0;
UINT64 insMemTouchedMax = 0;
INT32 immediateMax = INT32_MIN;
INT32 immediateMin = INT32_MAX;
ADDRDELTA displacementMax = INT32_MIN;
ADDRDELTA displacementMin = INT32_MAX;

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

#define RECORDFOOTPRINT(startaddr, size, container)                                                                                         \
    do                                                                                                                                      \
    {                                                                                                                                       \
        for (UINT64 addr = (UINT64)startaddr / 32; addr < ((UINT64)startaddr + size) / 32 + (((UINT64)startaddr + size) % 32 != 0); addr++) \
            container.insert(addr);                                                                                                         \
    } while (0)

#define RECORDMEM                                          \
    do                                                     \
    {                                                      \
        *insTypeAddr += 1;                                 \
        instMetrics->numLoads += numLoads;                 \
        instMetrics->numStores += numStores;               \
        insMemOperandsMap[readOperands + writeOperands]++; \
        insMemReadMap[readOperands]++;                     \
        insMemWriteMap[writeOperands]++;                   \
        if (displacementMin > insDisplacementMin)          \
            displacementMin = insDisplacementMin;          \
        if (displacementMax < insDisplacementMax)          \
            displacementMax = insDisplacementMax;          \
    } while (0)

#define PREDICATED_MEM_BASE_SIGNATURE UINT64 *insTypeAddr, UINT32 numLoads, UINT32 numStores, UINT32 readOperands, UINT32 writeOperands, ADDRDELTA insDisplacementMax, ADDRDELTA insDisplacementMin

VOID PredicatedAnalysisMetrics1Mem(PREDICATED_MEM_BASE_SIGNATURE, void *memOpAddr1, UINT32 memOpSize1)
{
    RECORDMEM;
    RECORDFOOTPRINT(memOpAddr1, memOpSize1, dataFootprint);
    insMemTouched += memOpSize1;
    if (memOpSize1 > insMemTouchedMax)
        insMemTouchedMax = memOpSize1;
}

VOID PredicatedAnalysisMetrics2Mem(PREDICATED_MEM_BASE_SIGNATURE, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2)
{
    RECORDMEM;
    RECORDFOOTPRINT(memOpAddr1, memOpSize1, dataFootprint);
    RECORDFOOTPRINT(memOpAddr2, memOpSize2, dataFootprint);
    insMemTouched += memOpSize1 + memOpSize2;
    if (memOpSize1 + memOpSize2 > insMemTouchedMax)
        insMemTouchedMax = memOpSize1 + memOpSize2;
}

VOID PredicatedAnalysisMetrics3Mem(PREDICATED_MEM_BASE_SIGNATURE, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3)
{
    RECORDMEM;
    RECORDFOOTPRINT(memOpAddr1, memOpSize1, dataFootprint);
    RECORDFOOTPRINT(memOpAddr2, memOpSize2, dataFootprint);
    RECORDFOOTPRINT(memOpAddr3, memOpSize3, dataFootprint);
    insMemTouched += memOpSize1 + memOpSize2 + memOpSize3;
    if (memOpSize1 + memOpSize2 + memOpSize3 > insMemTouchedMax)
        insMemTouchedMax = memOpSize1 + memOpSize2 + memOpSize3;
}

VOID PredicatedAnalysisMetrics4Mem(PREDICATED_MEM_BASE_SIGNATURE, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3, void *memOpAddr4, UINT32 memOpSize4)
{
    RECORDMEM;
    RECORDFOOTPRINT(memOpAddr1, memOpSize1, dataFootprint);
    RECORDFOOTPRINT(memOpAddr2, memOpSize2, dataFootprint);
    RECORDFOOTPRINT(memOpAddr3, memOpSize3, dataFootprint);
    RECORDFOOTPRINT(memOpAddr4, memOpSize4, dataFootprint);
    insMemTouched += memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4;
    if (memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4 > insMemTouchedMax)
        insMemTouchedMax = memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4;
}

VOID PredicatedAnalysisMetrics5Mem(PREDICATED_MEM_BASE_SIGNATURE, void *memOpAddr1, UINT32 memOpSize1, void *memOpAddr2, UINT32 memOpSize2, void *memOpAddr3, UINT32 memOpSize3, void *memOpAddr4, UINT32 memOpSize4, void *memOpAddr5, UINT32 memOpSize5)
{
    RECORDMEM;
    RECORDFOOTPRINT(memOpAddr1, memOpSize1, dataFootprint);
    RECORDFOOTPRINT(memOpAddr2, memOpSize2, dataFootprint);
    RECORDFOOTPRINT(memOpAddr3, memOpSize3, dataFootprint);
    RECORDFOOTPRINT(memOpAddr4, memOpSize4, dataFootprint);
    RECORDFOOTPRINT(memOpAddr5, memOpSize5, dataFootprint);
    insMemTouched += memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4 + memOpSize5;
    if (memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4 + memOpSize5 > insMemTouchedMax)
        insMemTouchedMax = memOpSize1 + memOpSize2 + memOpSize3 + memOpSize4 + memOpSize5;
}

VOID PredicatedAnalysisMetrics(UINT64 *insTypeAddr)
{
    *insTypeAddr += 1;
    insMemOperandsMap[0]++;
    insMemReadMap[0]++;
    insMemWriteMap[0]++;
}

VOID AnalysisMetrics(void *insAddr, UINT32 insSize, UINT32 operandsCount, UINT32 regReadCount, UINT32 regWriteCount, INT32 insImmediateMin, INT32 insImmediateMax)
{
    RECORDFOOTPRINT(insAddr, insSize, insFootprint);
    insLengthMap[insSize]++;
    insOperandsMap[operandsCount]++;
    insRegReadMap[regReadCount]++;
    insRegWriteMap[regWriteCount]++;
    if (insImmediateMin < immediateMin)
        immediateMin = insImmediateMin;
    if (insImmediateMax > immediateMax)
        immediateMax = insImmediateMax;
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

#define MEM_ANALYSIS_ARGUMENTS IARG_PTR, instypeaddr, IARG_UINT32, numLoads, IARG_UINT32, numStores, \
                               IARG_UINT32, readOperands, IARG_UINT32, writeOperands,                \
                               IARG_ADDRINT, insDisplacementMax, IARG_ADDRINT, insDisplacementMin

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
            ADDRDELTA insDisplacementMax = INT32_MIN, insDisplacementMin = INT32_MAX, displacementValue;

            for (UINT32 memOp = 0; memOp < memOperands; memOp++)
            {
                dataSize = INS_MemoryOperandSize(ins, memOp);
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
                displacementValue = INS_OperandMemoryDisplacement(ins, memOp);
                if (displacementValue > insDisplacementMax)
                    insDisplacementMax = displacementValue;
                if (displacementValue < insDisplacementMin)
                    insDisplacementMin = displacementValue;
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

            UINT32 numOperand = INS_OperandCount(ins);
            INT32 insImmediateMin = INT32_MAX, insImmediateMax = INT32_MIN, immediateValue;

            for (UINT32 i = 0; i < numOperand; i++)
            {
                if (INS_OperandIsImmediate(ins, i))
                {
                    immediateValue = (INT32)INS_OperandImmediate(ins, i);
                    if (immediateValue < insImmediateMin)
                        insImmediateMin = immediateValue;
                    if (immediateValue > insImmediateMax)
                        insImmediateMax = immediateValue;
                }
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            switch (memOperands)
            {
            case 0:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics,
                    IARG_PTR, instypeaddr, IARG_END);
                break;
            case 1:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics1Mem,
                    MEM_ANALYSIS_ARGUMENTS,
                    IARG_MEMORYOP_EA, 0, IARG_MEMORYOP_SIZE, 0, IARG_END);
                break;
            case 2:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics2Mem,
                    MEM_ANALYSIS_ARGUMENTS,
                    IARG_MEMORYOP_EA, 0, IARG_MEMORYOP_SIZE, 0,
                    IARG_MEMORYOP_EA, 1, IARG_MEMORYOP_SIZE, 1, IARG_END);
                break;
            case 3:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics3Mem,
                    MEM_ANALYSIS_ARGUMENTS,
                    IARG_MEMORYOP_EA, 0, IARG_MEMORYOP_SIZE, 0,
                    IARG_MEMORYOP_EA, 1, IARG_MEMORYOP_SIZE, 1,
                    IARG_MEMORYOP_EA, 2, IARG_MEMORYOP_SIZE, 2, IARG_END);
                break;
            case 4:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics4Mem,
                    MEM_ANALYSIS_ARGUMENTS,
                    IARG_MEMORYOP_EA, 0, IARG_MEMORYOP_SIZE, 0,
                    IARG_MEMORYOP_EA, 1, IARG_MEMORYOP_SIZE, 1,
                    IARG_MEMORYOP_EA, 2, IARG_MEMORYOP_SIZE, 2,
                    IARG_MEMORYOP_EA, 3, IARG_MEMORYOP_SIZE, 3, IARG_END);
                break;
            case 5:
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics5Mem,
                    MEM_ANALYSIS_ARGUMENTS,
                    IARG_MEMORYOP_EA, 0, IARG_MEMORYOP_SIZE, 0,
                    IARG_MEMORYOP_EA, 1, IARG_MEMORYOP_SIZE, 1,
                    IARG_MEMORYOP_EA, 2, IARG_MEMORYOP_SIZE, 2,
                    IARG_MEMORYOP_EA, 3, IARG_MEMORYOP_SIZE, 3,
                    IARG_MEMORYOP_EA, 4, IARG_MEMORYOP_SIZE, 4, IARG_END);
                break;
            default:
                *out << "ERROR: more than 5 memory operands" << endl;
                *out << "ERROR: " << INS_Disassemble(ins) << endl;
                INS_InsertThenPredicatedCall(
                    ins, IPOINT_BEFORE, (AFUNPTR)PredicatedAnalysisMetrics,
                    IARG_PTR, instypeaddr, IARG_END); // to prevent pin error
                break;
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckFastForward, IARG_END);
            INS_InsertThenCall(
                ins, IPOINT_BEFORE, (AFUNPTR)AnalysisMetrics,
                IARG_INST_PTR, IARG_UINT32, INS_Size(ins),
                IARG_UINT32, INS_OperandCount(ins),
                IARG_UINT32, INS_MaxNumRRegs(ins),
                IARG_UINT32, INS_MaxNumWRegs(ins),
                IARG_ADDRINT, insImmediateMin,
                IARG_ADDRINT, insImmediateMax,
                IARG_END);
        }
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)DoInsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

#define PRINT_METRICS(name, total)                                                    \
    std::setw(10) << std::right << name << " (" << std::fixed << std::setprecision(2) \
                  << std::setw(5) << std::right << (100.0 * name / total) << "%"      \
                  << ")" << std::endl

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
    *out << "HW2 analysis results from " << KnobOutputFile.Value() << endl;
    *out << "Number of instructions: " << insCount << endl;
    *out << "Fast forward at:        " << fastForward << endl;
    *out << "Number of instructions after fast forward: " << insCount - fastForward << endl;
    *out << "\n=====================PARTA=====================" << endl;
    *out << std::setw(35) << std::left << "Number of loads:" << PRINT_METRICS(instMetrics->numLoads, total);
    *out << std::setw(35) << std::left << "Number of stores:" << PRINT_METRICS(instMetrics->numStores, total);
    *out << std::setw(35) << std::left << "Number of nops:" << PRINT_METRICS(instMetrics->numNops, total);
    *out << std::setw(35) << std::left << "Number of direct calls:" << PRINT_METRICS(instMetrics->numDirectCalls, total);
    *out << std::setw(35) << std::left << "Number of indirect calls:" << PRINT_METRICS(instMetrics->numIndirectCalls, total);
    *out << std::setw(35) << std::left << "Number of returns:" << PRINT_METRICS(instMetrics->numReturns, total);
    *out << std::setw(35) << std::left << "Number of unconditional branches:" << PRINT_METRICS(instMetrics->numUncondBranches, total);
    *out << std::setw(35) << std::left << "Number of conditional branches:" << PRINT_METRICS(instMetrics->numCondBranches, total);
    *out << std::setw(35) << std::left << "Number of logical operations:" << PRINT_METRICS(instMetrics->numLogicalOps, total);
    *out << std::setw(35) << std::left << "Number of rotate/shift operations:" << PRINT_METRICS(instMetrics->numRotateShift, total);
    *out << std::setw(35) << std::left << "Number of flag operations:" << PRINT_METRICS(instMetrics->numFlagOps, total);
    *out << std::setw(35) << std::left << "Number of vector operations:" << PRINT_METRICS(instMetrics->numVector, total);
    *out << std::setw(35) << std::left << "Number of conditional moves:" << PRINT_METRICS(instMetrics->numCondMoves, total);
    *out << std::setw(35) << std::left << "Number of MMX/SSE operations:" << PRINT_METRICS(instMetrics->numMMXSSE, total);
    *out << std::setw(35) << std::left << "Number of system calls:" << PRINT_METRICS(instMetrics->numSysCalls, total);
    *out << std::setw(35) << std::left << "Number of FP operations:" << PRINT_METRICS(instMetrics->numFP, total);
    *out << std::setw(35) << std::left << "Number of other instructions:" << PRINT_METRICS(instMetrics->numRest, total);
    *out << "\n=====================PARTB=====================" << endl;
    // CPI numloads You should charge each load and store operation a fixed latency of
    // seventy cycles and every other instruction a latency of one cycle.
    FLT64 cpi = (instMetrics->numLoads + instMetrics->numStores) * 70.0 / total + (total - instMetrics->numLoads - instMetrics->numStores) * 1.0 / total;
    *out << "CPI: " << cpi << endl;
    *out << "\n=====================PARTC=====================" << endl;
    // Instruction footprint
    *out << "Number of 32 bytes region for data " << dataFootprint.size() << endl;
    *out << "Size of region is " << dataFootprint.size() * 32 << " bytes" << endl;
    *out << "Number of 32 bytes region for instructions " << insFootprint.size() << endl;
    *out << "Size of region is " << insFootprint.size() * 32 << " bytes " << endl;
    *out << "\n=====================PARTD=====================" << endl;

    // Instruction length and frequency
    *out << "\nD1 Distribution of instruction length (All Ins)" << endl;
    for (auto it = insLengthMap.begin(); it != insLengthMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " bytes: " << it->second << endl;

    // operands count and frequency
    *out << "\nD2 Distribution of the number of operands in an instruction (All Ins)" << endl;
    for (auto it = insOperandsMap.begin(); it != insOperandsMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " operands: " << it->second << endl;

    // register read operands
    *out << "\nD3 Distribution of the number of register read operands in an instruction (All Ins)" << endl;
    for (auto it = insRegReadMap.begin(); it != insRegReadMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " register read operands: " << it->second << endl;

    // register write operands
    *out << "\nD4 Distribution of the number of register write operands in an instruction (All Ins)" << endl;
    for (auto it = insRegWriteMap.begin(); it != insRegWriteMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " register write operands: " << it->second << endl;

    // memory operands
    UINT64 memins = 0;
    *out << "\nD5 Distribution of the number of memory operands in an instruction (Predicated Ins)" << endl;
    for (auto it = insMemOperandsMap.begin(); it != insMemOperandsMap.end(); it++)
    {
        *out << "Number of Instruction of " << it->first << " memory operands: " << it->second << endl;
        if (it->first > 0)
            memins += it->second;
    }

    // memory read operands
    *out << "\nD6 Distribution of the number of memory read operands in an instruction (Predicated Ins)" << endl;
    for (auto it = insMemReadMap.begin(); it != insMemReadMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " memory read operands: " << it->second << endl;

    // memory write operands
    *out << "\nD7 Distribution of the number of memory write operands in an instruction (Predicated Ins)" << endl;
    for (auto it = insMemWriteMap.begin(); it != insMemWriteMap.end(); it++)
        *out << "Number of Instruction of " << it->first << " memory write operands: " << it->second << endl;

    // memory touched
    *out << "\nD8 Maximum and average number of memory bytes touched by any memory instruction (Predicated Ins)" << endl;
    *out << "Maximum number of memory bytes touched: " << insMemTouchedMax << endl;
    *out << "Average number of memory bytes touched: " << insMemTouched * 1.0 / memins << endl;

    *out << "\nD9 Maximum and minimum values of the immediate field in an instruction." << endl;
    *out << "Maximum value of the immediate field: " << immediateMax << endl;
    *out << "Minimum value of the immediate field: " << immediateMin << endl;

    *out << "\nD10 Maximum and minimum values of the displacement field in a memory instruction(Predicated Ins)" << endl;
    *out << "Maximum value of the displacement field: " << displacementMax << endl;
    *out << "Minimum value of the displacement field: " << displacementMin << endl;

    *out << "===============================================" << endl;

    *out << "\nFor General max-min:" << endl;
    *out << "INT32_MAX = " << INT32_MAX << endl;
    *out << "INT32_MIN = " << INT32_MIN << endl;

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
