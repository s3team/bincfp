/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 * Sample buffering tool
 * 
 * This tool collects control flow profile
 * by filling a buffer.  When the buffer overflows,the callback writes all
 * of the collected records to a file.
 *
 * Count REP prefixed instructions and their repeat counts
 * "pin -xyzzy -txrep 0" don't translate rep into loop
 *
 * DEP: Detailed Execution Profile
 *
 */

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stddef.h>

#include "pin.H"
#include "portability.H"
#include "InstLib/instlib.H"
#include "InstLib/branch_pred.h"

using namespace INSTLIB;


/* thread context definition */
/*
typedef struct {
UINT16 prev_H_tag;
} thread_ctx_t;

REG thread_ctx_ptr;
*/

FILTER filter;
UINT32 totalBuffersFilled = 0;
UINT64 totalElementsProcessed = 0;
UINT16 prev_H_tag =-1;


/*
 *  Contains knobs to filter out things to instrument
 */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "bb-rep.out", "output file");
KNOB<BOOL> KnobDumpBuffer(KNOB_MODE_WRITEONCE, "pintool", "dump_buffers", "0", "dump the filled buffers to thread specific file");
KNOB<UINT32> KnobNumPagesInBuffer(KNOB_MODE_WRITEONCE, "pintool", "num_pages_in_buffer", "16384", "number of pages in buffer"); //buffer size 64M, 6897664 elements
KNOB<UINT32> KnobNumBBsInTrace(KNOB_MODE_WRITEONCE, "pintool", "num_bbs_in_trace", "8", "limit of BBs per trace");




/*
 * The ID of the buffer
 */
BUFFER_ID bufId;

/*
 * Thread specific data
 */
TLS_KEY mlog_key;

/*
 * Number of OS pages for the buffer
 */
//#define NUM_BUF_PAGES 1024


/*
 * Record of memory references.  Rather than having two separate
 * buffers for reads and writes, we just use one struct that includes a
 * flag for type.
 */
struct TRACEREF
{
    UINT16      tag;
};

/*
 * MLOG - thread specific data that is not handled by the buffering API.
 */
class MLOG
{
  public:
    MLOG(THREADID tid);
    ~MLOG();

    VOID DumpBufferToFile( struct TRACEREF * reference, UINT64 numElements, THREADID tid );

    UINT32 NumBuffersFilled() {return _numBuffersFilled;}
    UINT32 NumElementsProcessed() {return _numElementsProcessed;}

  private:
    UINT32 _numBuffersFilled;
    UINT32 _numElementsProcessed;
    ofstream _ofile;
};


MLOG::MLOG(THREADID tid)
{

    _numBuffersFilled = 0;
    _numElementsProcessed = 0;
	string filename = KnobOutputFile.Value() + "." + decstr(getpid_portable()) + "." + decstr(tid);
    
    _ofile.open(filename.c_str());
    
    if ( ! _ofile )
    {
        cerr << "Error: could not open output file." << endl;
        exit(1);
    }
    
    _ofile << hex;
}


MLOG::~MLOG()
{
    _ofile.close();
}


VOID MLOG::DumpBufferToFile( struct TRACEREF * reference, UINT64 numElements, THREADID tid )
{

	_numBuffersFilled++;
	_numElementsProcessed += (UINT32)numElements;

	if (!KnobDumpBuffer)
    {
        return;
    }
  
  for(UINT64 i=0; i<numElements; i++, reference++)
	//for(UINT64 i=0; i<numElements; i++, reference++)
    {
     //   if (reference->ea != 0)
     //        _ofile << reference->traceId << "   " << reference->traceCount << endl;
     _ofile << reference->tag<<endl;
    }
}


// Trivial analysis routine to pass its argument back in an IfCall so that we can use it
// to control the next piece of instrumentation.
static ADDRINT returnArg (BOOL arg)
{
    return arg;
}


/**************************************************************************
 *
 *  Instrumentation routines
 *
 **************************************************************************/

/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
VOID Trace(TRACE trace, VOID *v)
{
    if (!filter.SelectTrace(trace))
        return;
       
  //  TRACE_InsertCall(trace, IPOINT_BEFORE, AFUNPTR(traceAddr), IARG_FAST_ANALYSIS_CALL, IARG_ADDRINT, TRACE_Address(trace), IARG_END);
  /*
    BBL bbl = TRACE_BblHead(trace);
    if (BBL_Valid(bbl))
    {
    	INS ins = BBL_InsHead(bbl);
    	if(INS_Valid(ins))
    	{
    		INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
    		 	 	     	                                         IARG_ADDRINT, TRACE_Address(trace), offsetof(struct TRACEREF, traceId),
    		 	 	     	                                       //  IARG_UINT32, TraceCount, offsetof(struct TRACEREF, traceCount),
    		 	 	     	       	 	     	                     IARG_END);
    	}
    }
  */

    UINT16 current_H_tag;
    UINT16 current_L_tag;
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl))
    {
    	current_H_tag =  BBL_Address(bbl)>>16;
    	current_L_tag =	 BBL_Address(bbl) & 0x0000ffff;
    	INS ins_head = BBL_InsHead(bbl);

    	//if(INS_Valid(ins))
    	if (prev_H_tag != current_H_tag)
    	{ 
    		prev_H_tag =current_H_tag;
    		INS_InsertFillBuffer(ins_head, IPOINT_BEFORE, bufId, IARG_ADDRINT, 0x0000, offsetof(struct TRACEREF, tag),  IARG_END);
    		INS_InsertFillBuffer(ins_head, IPOINT_BEFORE, bufId, IARG_ADDRINT, current_H_tag, offsetof(struct TRACEREF, tag),  IARG_END);
    	}
    	if (current_L_tag ==0x0000)
    	{
    		INS_InsertFillBuffer(ins_head, IPOINT_BEFORE, bufId, IARG_ADDRINT, 0x0000, offsetof(struct TRACEREF, tag),  IARG_END);
    		INS_InsertFillBuffer(ins_head, IPOINT_BEFORE, bufId, IARG_ADDRINT, 0x0000, offsetof(struct TRACEREF, tag),  IARG_END);
    	}
    	if (current_L_tag !=0x0000)
    		INS_InsertFillBuffer(ins_head, IPOINT_BEFORE, bufId, IARG_ADDRINT, current_L_tag, offsetof(struct TRACEREF, tag),  IARG_END);

    	// Count REP prefixed instructions and their repeat counts
    	INS ins_tail = BBL_InsTail(bbl); //Last instruction of bbl
      	if (!INS_HasRealRep(ins_tail))
        continue;
      	INS_InsertIfCall(ins_tail, IPOINT_BEFORE, (AFUNPTR)returnArg, IARG_FIRST_REP_ITERATION, IARG_END);
      	INS_InsertFillBufferThen(ins_tail, IPOINT_BEFORE, bufId, IARG_REG_VALUE, INS_RepCountRegister(ins_tail), offsetof(struct TRACEREF, tag),  IARG_END);

    }


 /*
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl))
    {
    	INS ins = BBL_InsTail(bbl); //Last instruction of bbl
     
      if (INS_IsBranchOrCall(ins) || INS_IsRet(ins))
      {
     	 	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) setCJMP,
                 IARG_FAST_ANALYSIS_CALL,
                 IARG_BRANCH_TAKEN,
                 IARG_END);
       INS_InsertFillBufferThen(ins, IPOINT_BEFORE, bufId,
 	 	     	                                         IARG_ADDRINT, TraceId, offsetof(struct TRACEREF, traceId),
 	 	     	                                       //  IARG_UINT32, TraceCount, offsetof(struct TRACEREF, traceCount),
 	 	     	       	 	     	                     IARG_END);

      }
    }
   */
}


/**************************************************************************
 *
 *  Callback Routines
 *
 **************************************************************************/

/*!
 * Called when a buffer fills up, or the thread exits, so we can process it or pass it off
 * as we see fit.
 * @param[in] id		buffer handle
 * @param[in] tid		id of owning thread
 * @param[in] ctxt		application context
 * @param[in] buf		actual pointer to buffer
 * @param[in] numElements	number of records
 * @param[in] v			callback value
 * @return  A pointer to the buffer to resume filling.
 */
VOID * BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
                  UINT64 numElements, VOID *v)
{
    struct TRACEREF * reference=(struct TRACEREF*)buf;

    MLOG * mlog = static_cast<MLOG*>( PIN_GetThreadData( mlog_key, tid ) );

    mlog->DumpBufferToFile( reference, numElements, tid );
    
    return buf;
}


/*
 * Note that opening a file in a callback is only supported on Linux systems.
 * See buffer-win.cpp for how to work around this issue on Windows.
 */
VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    // There is a new MLOG for every thread.  Opens the output file.
    MLOG * mlog = new MLOG(tid);

    // A thread will need to look up its MLOG, so save pointer in TLS
    PIN_SetThreadData(mlog_key, mlog, tid);

	/* thread context pointer (ptr) */
    /*
	thread_ctx_t *tctx = NULL;
	if (unlikely((tctx = (thread_ctx_t *)calloc(1,
					sizeof(thread_ctx_t))) == NULL)) {
		LOG(string(__func__) + ": thread_ctx_t allocation failed\n");
	}
	PIN_SetContextReg(ctxt, thread_ctx_ptr, (ADDRINT)tctx);
    */

}


VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    MLOG * mlog = static_cast<MLOG*>(PIN_GetThreadData(mlog_key, tid));
    totalBuffersFilled += mlog->NumBuffersFilled();
    totalElementsProcessed +=  mlog->NumElementsProcessed();
    printf ("totalBuffersFilled %u\ntotalElementsProcessed %14.0f\n", (totalBuffersFilled),
              static_cast<double>(totalElementsProcessed));
    fflush (stdout);
    delete mlog;

    PIN_SetThreadData(mlog_key, 0, tid);

	/* get the thread context */
    /*
	thread_ctx_t *tctx = (thread_ctx_t *)
		PIN_GetContextReg(ctxt, thread_ctx_ptr);
	free(tctx);
	*/
}


/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool demonstrates the basic use of the buffering API." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID LimitTraces()
{
    CODECACHE_ChangeMaxBblsPerTrace(KnobNumBBsInTrace);
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
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
    if(unlikely(PIN_Init(argc,argv)))
    {
        return Usage();
    }
    
    // Initialize the memory reference buffer;
    // set up the callback to process the buffer.
    //
    bufId = PIN_DefineTraceBuffer(sizeof(struct TRACEREF), KnobNumPagesInBuffer,
                                  BufferFull, 0);

    if(unlikely(bufId == BUFFER_ID_INVALID))
    {
        cerr << "Error: could not allocate initial buffer" << endl;
        return 1;
    }

   // Obtain  a key for TLS storage.
    mlog_key = PIN_CreateThreadDataKey(0);

    CODECACHE_AddCacheInitFunction(LimitTraces, 0);

    /*
    thread_ctx_ptr = PIN_ClaimToolRegister();
    if (unlikely(!(REG_valid(thread_ctx_ptr))))
    {
        std::cerr << "Cannot allocate a scratch register.\n";
        std::cerr << std::flush;
        return 1;
    }
    */
    // add callbacks
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    
    
        // add an instrumentation function
    TRACE_AddInstrumentFunction(Trace, 0);
    
    filter.Activate();

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}


