#include <iostream>
#include <fstream>
#include <set>
#include "pin.H"
#include <cstdlib>

/* Macro and type definitions */
#define BILLION 1000000000
#define MAX_MEM_OPS 7
#define BLOCK_SIZE_LOG 6

using namespace std;

/* Global variables */
std::ostream *out = &cerr;

UINT64 icount = 0;	   // number of dynamically executed instructions
UINT64 fastForwardIns; // number of instruction to fast forward

/* Command line switches */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for HW1 output");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "number of instructions to fast forward in billions");

/* Utilities */

/* Print out help message. */
INT32 Usage()
{
	cerr << "CS422 Homework 4" << endl;
	cerr << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}

VOID InsCount(void)
{
	icount++;
}

ADDRINT FastForward(void)
{
	return (icount >= fastForwardIns && icount < fastForwardIns + BILLION);
}

ADDRINT CheckTerminate(void)
{
	return (icount >= fastForwardIns + BILLION);
}

VOID Terminate(void)
{
	PIN_ExitApplication(0);
}

VOID recordFootPrint(ADDRINT addr, UINT32 size)
{
	ADDRINT block = addr >> BLOCK_SIZE_LOG;
	ADDRINT last_block = (addr + size - 1) >> BLOCK_SIZE_LOG;
	do
	{
		block;

		block++;
	} while (block <= last_block);
}

VOID RecordInsType1(ADDRINT memOp1Addr, UINT32 memOp1Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
}

VOID RecordInsType2(ADDRINT memOp1Addr, UINT32 memOp1Size, ADDRINT memOp2Addr, UINT32 memOp2Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
	recordFootPrint(memOp2Addr, memOp2Size);
}

VOID RecordInsType3(ADDRINT memOp1Addr, UINT32 memOp1Size, ADDRINT memOp2Addr, UINT32 memOp2Size,
					ADDRINT memOp3Addr, UINT32 memOp3Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
	recordFootPrint(memOp2Addr, memOp2Size);
	recordFootPrint(memOp3Addr, memOp3Size);
}

VOID RecordInsType4(ADDRINT memOp1Addr, UINT32 memOp1Size, ADDRINT memOp2Addr, UINT32 memOp2Size,
					ADDRINT memOp3Addr, UINT32 memOp3Size, ADDRINT memOp4Addr, UINT32 memOp4Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
	recordFootPrint(memOp2Addr, memOp2Size);
	recordFootPrint(memOp3Addr, memOp3Size);
	recordFootPrint(memOp4Addr, memOp4Size);
}

VOID RecordInsType5(ADDRINT memOp1Addr, UINT32 memOp1Size, ADDRINT memOp2Addr, UINT32 memOp2Size,
					ADDRINT memOp3Addr, UINT32 memOp3Size, ADDRINT memOp4Addr, UINT32 memOp4Size,
					ADDRINT memOp5Addr, UINT32 memOp5Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
	recordFootPrint(memOp2Addr, memOp2Size);
	recordFootPrint(memOp3Addr, memOp3Size);
	recordFootPrint(memOp4Addr, memOp4Size);
	recordFootPrint(memOp5Addr, memOp5Size);
}

VOID RecordInsType6(ADDRINT memOp1Addr, UINT32 memOp1Size, ADDRINT memOp2Addr, UINT32 memOp2Size,
					ADDRINT memOp3Addr, UINT32 memOp3Size, ADDRINT memOp4Addr, UINT32 memOp4Size,
					ADDRINT memOp5Addr, UINT32 memOp5Size, ADDRINT memOp6Addr, UINT32 memOp6Size)
{
	recordFootPrint(memOp1Addr, memOp1Size);
	recordFootPrint(memOp2Addr, memOp2Size);
	recordFootPrint(memOp3Addr, memOp3Size);
	recordFootPrint(memOp4Addr, memOp4Size);
	recordFootPrint(memOp5Addr, memOp5Size);
	recordFootPrint(memOp6Addr, memOp6Size);
}

/*
 * Array containing pointers to analysis functions for collecting traces. Different
 * analysis functions are called depending on the number of memory operands. Currently,
 * instructions with upto four memory operands are supported. I do not thinks x86 has
 * instructions with more than 4 memory operands. But for completeness, I have added
 * support for upto 6 memory operands.
 */
AFUNPTR RecordInsTypeFuncs[] = {
	AFUNPTR(NULL),
	AFUNPTR(RecordInsType1),
	AFUNPTR(RecordInsType2),
	AFUNPTR(RecordInsType3),
	AFUNPTR(RecordInsType4),
	AFUNPTR(RecordInsType5),
	AFUNPTR(RecordInsType6)};

/* Instruction instrumentation routine */
VOID Instruction(INS ins, VOID *v)
{
	UINT32 memOps = 0;
	UINT32 effectiveMemOps = 0;

	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)CheckTerminate, IARG_END);
	INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);

	memOps = INS_MemoryOperandCount(ins);
	if (memOps)
	{
		/* Create a list for analysis arguments */
		IARGLIST args = IARGLIST_Alloc();

		for (UINT32 memOp = 0; memOp < memOps; memOp++)
		{
			if (INS_MemoryOperandIsRead(ins, memOp))
			{
				/* Add new memory operand. */
				effectiveMemOps++;
				IARGLIST_AddArguments(args,
									  IARG_MEMORYOP_EA, memOp,	 // Effective memory address
									  IARG_MEMORYOP_SIZE, memOp, // memory access size
									  IARG_END);
			}
		}

		for (UINT32 memOp = 0; memOp < memOps; memOp++)
		{
			if (INS_MemoryOperandIsWritten(ins, memOp))
			{
				/* Add new memory operand. */
				effectiveMemOps++;
				IARGLIST_AddArguments(args,
									  IARG_MEMORYOP_EA, memOp,	 // Effective memory address
									  IARG_MEMORYOP_SIZE, memOp, // memory access size
									  IARG_END);
			}
		}

		ASSERT(effectiveMemOps > 0, "Number of effective memory operands is zero!");
		ASSERT(effectiveMemOps < MAX_MEM_OPS, "Number of effective memory operands exceeded MAX_MEM_OPS!");

		INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
		INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordInsTypeFuncs[effectiveMemOps],
									 IARG_IARGLIST, args, IARG_END);

		/* Free analysis argument list */
		IARGLIST_Free(args);
	}

	/* Called for each instruction */
	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_END);
}

/* Fini routine */
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
	*out << "HW4 analysis results from " << KnobOutputFile.Value() << endl;
	*out << "Number of instructions: " << icount << endl;
	*out << "Fast forward at:        " << fastForwardIns << endl;
	*out << "Number of instructions after fast forward: " << icount - fastForwardIns << endl;

	*out << "=====================================================================" << endl;

	*out << "=====================================================================" << endl;
}

int main(int argc, char *argv[])
{
	// Initialize PIN library. Print help message if -h(elp) is specified
	// in the command line or the command line is invalid
	if (PIN_Init(argc, argv))
		return Usage();

	/* Set number of instructions to fast forward and simulate */
	fastForwardIns = KnobFastForward.Value() * BILLION;

	string fileName = KnobOutputFile.Value();

	if (!fileName.empty())
		out = new std::ofstream(fileName.c_str());

	// Register function to be called to instrument instructions
	INS_AddInstrumentFunction(Instruction, 0);

	// Register function to be called when the application exits
	PIN_AddFiniFunction(Fini, 0);

	cerr << "=====================================================================" << endl;
	cerr << "This application is instrumented by HW4" << endl;
	if (!KnobOutputFile.Value().empty())
		cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
	cerr << "=====================================================================" << endl;

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
