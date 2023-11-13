#include <iostream>
#include <fstream>
#include <set>
#include "pin.H"
#include <cstdlib>

/* Macro and type definitions */
#define BILLION 1000000000
#define MAX_INS_SIZE 20
#define MAX_MEM_OPS 5
#define MAX_INS_OPS 10
#define MEM_LATENCY 70.0

using namespace std;

typedef enum
{
	INS_LOAD = 0,
	INS_STORE,
	INS_NOP,
	INS_DIRECT_CALL,
	INS_INDIRECT_CALL,
	INS_RETURN,
	INS_UNCOND_BRANCH,
	INS_COND_BRANCH,
	INS_LOGICAL,
	INS_ROTATE_SHIFT,
	INS_FLAGOP,
	INS_VECTOR,
	INS_CMOV,
	INS_MMX_SSE,
	INS_SYSCALL,
	INS_FP,
	INS_OTHERS,
	INS_COUNT
} INS_CATEGORY;

string insNames[] = {
	"Loads",
	"Stores",
	"NOPs",
	"Direct calls",
	"Indirect calls",
	"Returns",
	"Unconditional branches",
	"Conditional branches",
	"Logical operations",
	"Rotate and Shift",
	"Flag operations",
	"Vector instructions",
	"Conditional moves",
	"MMX and SSE instructions",
	"System calls",
	"Floating point instructions",
	"The rest"
};


/* Global variables */
std::ostream * out = &cerr;

typedef std::set<ADDRINT> FOOTPRINT;
FOOTPRINT insFootPrint;
FOOTPRINT memFootPrint;

UINT64 icount = 0; //number of dynamically executed instructions
UINT64 insTypeCount[INS_COUNT];

UINT64 fastForwardIns; // number of instruction to fast forward
UINT64 maxIns; // maximum number of instructions to simulate

UINT64 insSizeCount[MAX_INS_SIZE]; // instruction size count
UINT64 insMemOpCount[MAX_MEM_OPS]; // instruction memory operand count
UINT64 insMemReadOpCount[MAX_MEM_OPS]; // instruction memory read operand count
UINT64 insMemWriteOpCount[MAX_MEM_OPS]; // instruction memory write operand count
UINT64 insOpCount[MAX_INS_OPS];	   // instruction operand count
UINT64 insRegReadOpCount[MAX_INS_OPS];    // instruction register source operand count
UINT64 insRegWriteOpCount[MAX_INS_OPS];    // instruction register destination operand count

UINT32 maxBytesTouched = 0;		// maximum number of bytes touched by an instruction
UINT64 totalBytesTouched = 0;		// total number of bytes touched by all instructions
INT32 maxImmediate = 0;			// maximum value of immediate field
INT32 minImmediate = 0xFFFFFFFFLL;	// minimum value of immediate field
ADDRDELTA maxDisplacement = 0;		// maximum value of displacement in memory addressing
ADDRDELTA minDisplacement = 0xFFFFFFFFLL;	// minimum value of displacement in memory addressing

/* Command line switches */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for HW1 output");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "number of instructions to fast forward in billions");


/* Utilities */

/* Print out help message. */
INT32 Usage()
{
	cerr << "CS422 Homework 1" << endl;
	cerr << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

VOID InsCount(void)
{
	icount++;
}

ADDRINT FastForward(void)
{
	return (icount >= fastForwardIns && icount < maxIns);
}

ADDRINT Terminate(void)
{
        return (icount >= maxIns);
}

VOID addFootPrint(FOOTPRINT &set, ADDRINT addr, UINT32 size)
{
	ADDRINT block = addr >> 5;
	ADDRINT last_block = (addr + size - 1) >> 5;
	do
	{
		set.insert(block);

		if (block == last_block)
			break;
		block++;
	} while (block < last_block);
}

VOID RecordIns(ADDRINT insAddr, UINT32 insSize, UINT32 opCount, UINT32 opRCount, UINT32 opWCount, INT32 maxImm, INT32 minImm)
{
	insOpCount[opCount]++;
	insRegReadOpCount[opRCount]++;
	insRegWriteOpCount[opWCount]++;
	insSizeCount[insSize]++;
	if (maxImm > maxImmediate) maxImmediate = maxImm;
	if (minImm < minImmediate) minImmediate = minImm;
	addFootPrint(insFootPrint, insAddr, insSize);
}

VOID RecordInsType0(INS_CATEGORY category)
{
	insTypeCount[category]++;
	insMemOpCount[0]++;
}

VOID RecordInsType1(INS_CATEGORY category,
	INS_CATEGORY memOp1Category, ADDRINT memOp1Addr, UINT32 memOp1Size, UINT32 readOpCount, UINT32 writeOpCount, ADDRDELTA maxdisp, ADDRDELTA mindisp)
{
	insTypeCount[memOp1Category] += (memOp1Size + 3) / 4;
	addFootPrint(memFootPrint, memOp1Addr, memOp1Size);

	insTypeCount[category]++;
	insMemOpCount[1]++;

	if (memOp1Size > maxBytesTouched) maxBytesTouched = memOp1Size;
	totalBytesTouched += memOp1Size;

	insMemReadOpCount[readOpCount]++;
        insMemWriteOpCount[writeOpCount]++;

	if (maxdisp > maxDisplacement) maxDisplacement = maxdisp;
	if (mindisp < minDisplacement) minDisplacement = mindisp;
}

VOID RecordInsType2(INS_CATEGORY category,
	INS_CATEGORY memOp1Category, ADDRINT memOp1Addr, UINT32 memOp1Size,
	INS_CATEGORY memOp2Category, ADDRINT memOp2Addr, UINT32 memOp2Size, UINT32 readOpCount, UINT32 writeOpCount, ADDRDELTA maxdisp, ADDRDELTA mindisp)
{
	UINT32 totalSize = memOp1Size+memOp2Size;
	insTypeCount[memOp1Category] += (memOp1Size + 3) / 4;
	addFootPrint(memFootPrint, memOp1Addr, memOp1Size);

	insTypeCount[memOp2Category] += (memOp2Size + 3) / 4;
	addFootPrint(memFootPrint, memOp2Addr, memOp2Size);

	insTypeCount[category]++;
	insMemOpCount[2]++;

	if (totalSize > maxBytesTouched) maxBytesTouched = totalSize;
	totalBytesTouched += totalSize;

	insMemReadOpCount[readOpCount]++;
        insMemWriteOpCount[writeOpCount]++;

	if (maxdisp > maxDisplacement) maxDisplacement = maxdisp;
	if (mindisp < minDisplacement) minDisplacement = mindisp;
}

VOID RecordInsType3(INS_CATEGORY category,
	INS_CATEGORY memOp1Category, ADDRINT memOp1Addr, UINT32 memOp1Size,
	INS_CATEGORY memOp2Category, ADDRINT memOp2Addr, UINT32 memOp2Size,
	INS_CATEGORY memOp3Category, ADDRINT memOp3Addr, UINT32 memOp3Size, UINT32 readOpCount, UINT32 writeOpCount, ADDRDELTA maxdisp, ADDRDELTA mindisp)
{
	UINT32 totalSize = memOp1Size+memOp2Size+memOp3Size;
	insTypeCount[memOp1Category] += (memOp1Size + 3) / 4;
	addFootPrint(memFootPrint, memOp1Addr, memOp1Size);

	insTypeCount[memOp2Category] += (memOp2Size + 3) / 4;
	addFootPrint(memFootPrint, memOp2Addr, memOp2Size);

	insTypeCount[memOp3Category] += (memOp3Size + 3) / 4;
	addFootPrint(memFootPrint, memOp3Addr, memOp3Size);

	insTypeCount[category]++;
	insMemOpCount[3]++;

	if (totalSize > maxBytesTouched) maxBytesTouched = totalSize;
        totalBytesTouched += totalSize;	

	insMemReadOpCount[readOpCount]++;
        insMemWriteOpCount[writeOpCount]++;

	if (maxdisp > maxDisplacement) maxDisplacement = maxdisp;
	if (mindisp < minDisplacement) minDisplacement = mindisp;
}

VOID RecordInsType4(INS_CATEGORY category,
	INS_CATEGORY memOp1Category, ADDRINT memOp1Addr, UINT32 memOp1Size,
	INS_CATEGORY memOp2Category, ADDRINT memOp2Addr, UINT32 memOp2Size,
	INS_CATEGORY memOp3Category, ADDRINT memOp3Addr, UINT32 memOp3Size,
	INS_CATEGORY memOp4Category, ADDRINT memOp4Addr, UINT32 memOp4Size, UINT32 readOpCount, UINT32 writeOpCount, ADDRDELTA maxdisp, ADDRDELTA mindisp)
{
	UINT32 totalSize = memOp1Size+memOp2Size+memOp3Size+memOp4Size;
	insTypeCount[memOp1Category] += (memOp1Size + 3) / 4;
	addFootPrint(memFootPrint, memOp1Addr, memOp1Size);

	insTypeCount[memOp2Category] += (memOp2Size + 3) / 4;
	addFootPrint(memFootPrint, memOp2Addr, memOp2Size);

	insTypeCount[memOp3Category] += (memOp3Size + 3) / 4;
	addFootPrint(memFootPrint, memOp3Addr, memOp3Size);

	insTypeCount[memOp4Category] += (memOp4Size + 3) / 4;
	addFootPrint(memFootPrint, memOp4Addr, memOp4Size);

	insTypeCount[category]++;
	insMemOpCount[4]++;

	if (totalSize > maxBytesTouched) maxBytesTouched = totalSize;
        totalBytesTouched += totalSize;

	insMemReadOpCount[readOpCount]++;
	insMemWriteOpCount[writeOpCount]++;

	if (maxdisp > maxDisplacement) maxDisplacement = maxdisp;
	if (mindisp < minDisplacement) minDisplacement = mindisp;
}

/* Array containing pointers to analysis functions for collecting traces. Different
 * analysis functions are called depending on the number of memory operands. Currently,
 * instructions with upto four memory operands are supported. I do not thinks x86 has
 * instructions with more than 4 memory operands. */
AFUNPTR RecordInsTypeFuncs[] = {
	AFUNPTR(RecordInsType0),
	AFUNPTR(RecordInsType1),
	AFUNPTR(RecordInsType2),
	AFUNPTR(RecordInsType3),
	AFUNPTR(RecordInsType4)
};

VOID StatDump(void)
{
	double cycles = 0;
	UINT64 totalIns = 0;
	
	*out << "===============================================" << endl;
        *out << "Instruction Type Results: " << endl;
	for (UINT32 i = 0; i < INS_COUNT; i++) {
		totalIns += insTypeCount[i];
	}
        for (UINT32 i = 0; i < INS_COUNT; i++) {
                *out << insNames[i] << ": " << insTypeCount[i] << " (" << (double)insTypeCount[i]/totalIns << ")" << endl;
		if ((i == INS_LOAD) || (i == INS_STORE)) cycles += (MEM_LATENCY*insTypeCount[i]);
		else cycles += insTypeCount[i];
	}
	*out << "CPI: " << cycles/totalIns << endl;

        *out << endl;
        *out << "Instruction Size Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_SIZE; i++)
                *out << i << " : " << insSizeCount[i] << endl;

        *out << endl;
        *out << "Memory Instruction Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemOpCount[i] << endl;

        *out << endl;
	*out << "Memory Instruction Read Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemReadOpCount[i] << endl;

        *out << endl;
	*out << "Memory Instruction Write Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemWriteOpCount[i] << endl;

        *out << endl;
	*out << "Instruction Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insOpCount[i] << endl;

        *out << endl;
	*out << "Instruction Register Read Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insRegReadOpCount[i] << endl;

        *out << endl;
	*out << "Instruction Register Write Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insRegWriteOpCount[i] << endl;

        *out << endl;
        *out << "Instruction Blocks Accesses : " << insFootPrint.size() << endl;
        *out << "Memory Blocks Accesses : " << memFootPrint.size() << endl;
	*out << "Maximum number of bytes touched by an instruction : " << maxBytesTouched << endl;
	*out << "Average number of bytes touched by an instruction : " << (double)totalBytesTouched/(insMemOpCount[1]+insMemOpCount[2]+insMemOpCount[3]+insMemOpCount[4]) << endl;
	*out << "Maximum value of immediate : " << maxImmediate << endl;
	*out << "Minimum value of immediate : " << minImmediate << endl;
	*out << "Maximum value of displacement used in memory addressing : " << maxDisplacement << endl;
	*out << "Minimum value of displacement used in memory addressing : " << minDisplacement << endl;
        *out << "===============================================" << endl;

	exit(0);
}

/* Instruction instrumentation routine */
VOID Instruction(INS ins, VOID *v)
{
	UINT32 memOps = 0;
	UINT32 effectiveMemOps = 0;
	UINT32 effectiveReadMemOps = 0;
	UINT32 effectiveWriteMemOps = 0;
	UINT32 ops = INS_OperandCount(ins);
	INT32 maxImm = 0, immValue, minImm = 0xFFFFFFFFLL;
	ADDRDELTA maxDisp = 0, dispValue, minDisp = 0xFFFFFFFFLL;
	INS_CATEGORY category;

	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) Terminate, IARG_END);
        INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) StatDump, IARG_END);

	/* Instruction footprint */
	ASSERT(INS_Size(ins) < MAX_INS_SIZE, "Instruction size exceeded MAX_INS_SIZE!");
	ASSERT(ops < MAX_INS_OPS, "Instruction op count exceeded MAX_INS_OPS!");
	for (UINT32 Op = 0; Op < ops; Op++) {
		if (INS_OperandIsImmediate(ins, Op)) {
			immValue = INS_OperandImmediate(ins, Op);
			if (immValue > maxImm) maxImm = immValue;
			if (immValue < minImm) minImm = immValue;
		}
		if (INS_OperandIsMemory (ins, Op)) {
			dispValue = INS_OperandMemoryDisplacement(ins, Op);
                        if (dispValue > maxDisp) maxDisp = dispValue;
                        if (dispValue < minDisp) minDisp = dispValue;
		}
	}
	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) RecordIns,
		IARG_ADDRINT, INS_Address(ins), // instruction address
		IARG_UINT32, INS_Size(ins), // instruction size
		IARG_UINT32, ops,	// instruction operand count
		IARG_UINT32, INS_MaxNumRRegs(ins),	// instruction register read operand count
		IARG_UINT32, INS_MaxNumWRegs(ins),	// instruction register write operand count
		IARG_ADDRINT, maxImm,		// instruction's maximum immediate value
		IARG_ADDRINT, minImm,			// instruction's minimum immediate value
		IARG_END);

	/* Calculate instruction type */
	switch (INS_Category(ins))
	{
		case XED_CATEGORY_NOP:
			category = INS_NOP;
			break;

		case XED_CATEGORY_CALL:
			if (INS_IsDirectCall(ins)) category = INS_DIRECT_CALL;
			else category = INS_INDIRECT_CALL;
			break;

		case XED_CATEGORY_RET:
			category = INS_RETURN;
			break;

		case XED_CATEGORY_UNCOND_BR:
			category = INS_UNCOND_BRANCH;
			break;

		case XED_CATEGORY_COND_BR:
			category = INS_COND_BRANCH;
			break;

		case XED_CATEGORY_LOGICAL:
			category = INS_LOGICAL;
			break;

		case XED_CATEGORY_ROTATE:
		case XED_CATEGORY_SHIFT:
			category = INS_ROTATE_SHIFT;
			break;

		case XED_CATEGORY_FLAGOP:
			category = INS_FLAGOP;
			break;

		case XED_CATEGORY_AVX:
		case XED_CATEGORY_AVX2:
		case XED_CATEGORY_AVX2GATHER:
		case XED_CATEGORY_AVX512:
			category = INS_VECTOR;
			break;

		case XED_CATEGORY_CMOV:
			category = INS_CMOV;
			break;

		case XED_CATEGORY_MMX:
		case XED_CATEGORY_SSE:
			category = INS_MMX_SSE;
			break;

		case XED_CATEGORY_SYSCALL:
			category = INS_SYSCALL;
			break;

		case XED_CATEGORY_X87_ALU:
			category = INS_FP;
			break;

		default:
			category = INS_OTHERS;
			break;
	}
	/* Check that category is valid */
	ASSERT(category > INS_STORE && category < INS_COUNT, "Invalid Ins Category!");

	/* Create a list for analysis arguments */
	IARGLIST args = IARGLIST_Alloc();

	/* Add category as the first argument to analysis routine */
	IARGLIST_AddArguments(args, IARG_UINT32, category, IARG_END);

	memOps = INS_MemoryOperandCount(ins);
	if (memOps)
	{
		for (UINT32 memOp = 0; memOp < memOps; memOp++)
		{
			if (INS_MemoryOperandIsRead(ins, memOp))
			{
				/* Add new memory operand. */
				effectiveMemOps++;
				effectiveReadMemOps++;
				IARGLIST_AddArguments(args,
					IARG_UINT32, INS_LOAD, // instruction type load
					IARG_MEMORYOP_EA, memOp, // Effective memory address
					IARG_UINT32, INS_MemoryOperandSize(ins, memOp), // memory access size
					IARG_END);
			}

			if (INS_MemoryOperandIsWritten(ins, memOp))
			{
				/* Add new memory operand. */
				effectiveMemOps++;
				effectiveWriteMemOps++;
				IARGLIST_AddArguments(args,
					IARG_UINT32, INS_STORE, // instruction type store
					IARG_MEMORYOP_EA, memOp, // Effective memory address
					IARG_UINT32, INS_MemoryOperandSize(ins, memOp), // memory access size
					IARG_END);
			}
		}
		IARGLIST_AddArguments(args, IARG_UINT32, effectiveReadMemOps, IARG_UINT32, effectiveWriteMemOps, IARG_ADDRINT, maxDisp, IARG_ADDRINT, minDisp, IARG_END);
	}
	ASSERT(effectiveMemOps < MAX_MEM_OPS, "Number of effective memory operands exceeded MAX_MEM_OPS!");

	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) FastForward, IARG_END);
	INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR) RecordInsTypeFuncs[effectiveMemOps],
		IARG_IARGLIST, args, IARG_END);

	/* Free analysis argument list */
	IARGLIST_Free(args);

	/* Called for each instruction */
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) InsCount, IARG_END);
}

/* Fini routine */
VOID Fini(INT32 code, VOID * v)
{
	double cycles = 0;
        UINT64 totalIns = 0;

        *out << "===============================================" << endl;
        *out << "Instruction Type Results: " << endl;
        for (UINT32 i = 0; i < INS_COUNT; i++) {
                totalIns += insTypeCount[i];
        }
        for (UINT32 i = 0; i < INS_COUNT; i++) {
                *out << insNames[i] << ": " << insTypeCount[i] << " (" << (double)insTypeCount[i]/totalIns << ")" << endl;
                if ((i == INS_LOAD) || (i == INS_STORE)) cycles += (MEM_LATENCY*insTypeCount[i]);
                else cycles += insTypeCount[i];
        }
        *out << "CPI: " << cycles/totalIns << endl;

        *out << endl;
        *out << "Instruction Size Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_SIZE; i++)
                *out << i << " : " << insSizeCount[i] << endl;

        *out << endl;
        *out << "Memory Instruction Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemOpCount[i] << endl;

        *out << endl;
	*out << "Memory Instruction Read Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemReadOpCount[i] << endl;

        *out << endl;
        *out << "Memory Instruction Write Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_MEM_OPS; i++)
                *out << i << " : " << insMemWriteOpCount[i] << endl;

        *out << endl;
	*out << "Instruction Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insOpCount[i] << endl;

        *out << endl;
	*out << "Instruction Register Read Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insRegReadOpCount[i] << endl;

        *out << endl;
        *out << "Instruction Register Write Operand Results: " << endl;
        for (UINT32 i = 0; i < MAX_INS_OPS; i++)
                *out << i << " : " << insRegWriteOpCount[i] << endl;

        *out << endl;
        *out << "Instruction Blocks Accesses : " << insFootPrint.size() << endl;
        *out << "Memory Blocks Accesses : " << memFootPrint.size() << endl;
        *out << "Maximum number of bytes touched by an instruction : " << maxBytesTouched << endl;
        *out << "Average number of bytes touched by an instruction : " << (double)totalBytesTouched/(insMemOpCount[1]+insMemOpCount[2]+insMemOpCount[3]+insMemOpCount[4]) << endl;
	*out << "Maximum value of immediate : " << maxImmediate << endl;
	*out << "Minimum value of immediate : " << minImmediate << endl;
        *out << "Maximum value of displacement used in memory addressing : " << maxDisplacement << endl;
	*out << "Minimum value of displacement used in memory addressing : " << minDisplacement << endl;
        *out << "===============================================" << endl;
}

int main(int argc, char *argv[])
{
	// Initialize PIN library. Print help message if -h(elp) is specified
	// in the command line or the command line is invalid 
	if (PIN_Init(argc, argv))
		return Usage();

	/* Set number of instructions to fast forward and simulate */
	fastForwardIns = KnobFastForward.Value() * BILLION;
	maxIns = fastForwardIns + BILLION;

	string fileName = KnobOutputFile.Value();

	if (!fileName.empty())
		out = new std::ofstream(fileName.c_str());

	for (UINT32 i = 0; i < INS_COUNT; i++) insTypeCount[i] = 0;
	for (UINT32 i = 0; i < MAX_INS_SIZE; i++) insSizeCount[i] = 0;
	for (UINT32 i = 0; i < MAX_MEM_OPS; i++) insMemOpCount[i] = 0;
	for (UINT32 i = 0; i < MAX_MEM_OPS; i++) insMemReadOpCount[i] = 0;
	for (UINT32 i = 0; i < MAX_MEM_OPS; i++) insMemWriteOpCount[i] = 0;
	for (UINT32 i = 0; i < MAX_INS_OPS; i++) insOpCount[i] = 0;
	for (UINT32 i = 0; i < MAX_INS_OPS; i++) insRegReadOpCount[i] = 0;
	for (UINT32 i = 0; i < MAX_INS_OPS; i++) insRegWriteOpCount[i] = 0;

	// Register function to be called to instrument instructions
	INS_AddInstrumentFunction(Instruction, 0);

	// Register function to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	cerr << "===============================================" << endl;
	cerr << "This application is instrumented by HW1" << endl;
	if (!KnobOutputFile.Value().empty())
		cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
	cerr << "===============================================" << endl;

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
