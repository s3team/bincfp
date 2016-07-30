/*
Todo:

*/


/*rolling repeated traces*/
#include <iostream>
#include <fstream>
#include "pin.H"
#include "InstLib/instlib.H"

using namespace INSTLIB;
FILTER filter;

FILE * trace;
UINT32 MASK=0;
UINT32 FLIP=0;
ADDRINT TraceAddr=0;
UINT32 PreviousTraceID=0;
UINT32 TraceCount=1;

/*
VOID PIN_FAST_ANALYSIS_CALL print(UINT32 taken) 
 { 
 	 fprintf(trace, "%d\n", taken);
 	 MASK=0;
 	 FLIP=0;
 }
*/
VOID print() 
 { 
 //	 fprintf(trace, "%d, trace_id is: ", FLIP);
 	 UINT32 addr_postfix = TraceAddr & 0x00ffffff;
 	 UINT32 trace_id = (addr_postfix<<8) | FLIP;
 	 if(trace_id != PreviousTraceID)
 	 {
 	 	if (TraceCount > 1)
 	 	{	
 	   fprintf(trace, "%x %d\n", PreviousTraceID, (TraceCount-1)); // first trace already output	 
 	   TraceCount=1;
 	  }
 	  
 	  fprintf(trace, "%x\n", trace_id);	
 	  PreviousTraceID = trace_id; 
 	  
 	 } 
 	else
 	 	{
 	 		TraceCount++;
 	 	}
 	 TraceAddr=0;
 	 MASK=0;
 	 FLIP=0;
 }

VOID PIN_FAST_ANALYSIS_CALL print2(ADDRINT addr) 
 {  
 	 
//  	fprintf(trace, "0x%x: ", addr);
  	TraceAddr = addr;
	} 
ADDRINT PIN_FAST_ANALYSIS_CALL setCJMP (UINT32 taken)
{
	//t1.flip =t1.flip<<1;
	FLIP |= (taken<<MASK);
  MASK+=1;	
  return taken;
}

// Pin calls this function every time a new basic block is encountered
// It inserts a call to docount
VOID Trace(TRACE trace, VOID *v)
{
   	  if (!filter.SelectTrace(trace))
        return;

    TRACE_InsertCall(trace, IPOINT_BEFORE, AFUNPTR(print2), IARG_FAST_ANALYSIS_CALL, IARG_ADDRINT, TRACE_Address(trace), IARG_END);
    // Visit every basic block  in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
      
       INS ins = BBL_InsTail(bbl); //Last instruction of bbl
      if (INS_IsBranchOrCall(ins) || INS_IsRet(ins))
      {
     
           	 	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) setCJMP,
                       IARG_FAST_ANALYSIS_CALL,
                       IARG_BRANCH_TAKEN,
                       IARG_END); 
             //IPOINT_TAKEN_BRANCH
              INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) print, IARG_END);          
      }
    }    
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool",
    "o", "cjmp-sequence3.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v)
{
    // Write to a file since cout and cerr maybe closed by the application
 //   fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID LimitTraces()
{
    CODECACHE_ChangeMaxBblsPerTrace(8);
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char * argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    trace = fopen("cjmp-sequence3.out", "w");
    CODECACHE_AddCacheInitFunction(LimitTraces, 0);
    
    // Register Instruction to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    filter.Activate();
    
    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
