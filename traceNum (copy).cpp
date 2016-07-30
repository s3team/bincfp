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
Todo:
 IARG_TYPE IARG_CALL_ORDER cannot be used with the Buffer APIs.
 BBNumber executed can not be synchronized with INS_InsertFillBufferThen

 */

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stddef.h>

#include "pin.H"
#include "portability.H"
#include "InstLib/instlib.H"

using namespace INSTLIB;

FILTER filter;
UINT32 MASK=0;
UINT32 FLIP=0;
ADDRINT TraceAddr;
//UINT32 TraceId;
UINT32 BBNumber;
//UINT32 PreviousTraceID=0;
//UINT32 TraceCount=1;
//UINT32 BranchTaken=0;

UINT32 totalBuffersFilled = 0;
UINT64 totalElementsProcessed = 0;



/*
 *  Contains knobs to filter out things to instrument
 */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "trace-num.out", "output file");
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
    UINT32      traceId;
//    UINT32      traceCount;
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
     _ofile << reference->traceId<<endl;
    }
}

VOID PIN_FAST_ANALYSIS_CALL traceAddr(ADDRINT addr) 
{
	TraceAddr = addr;
	BBNumber=1;
} 

UINT32 PIN_FAST_ANALYSIS_CALL setCJMP (UINT32 taken)
{
	//t1.flip =t1.flip<<1;
  BBNumber+=(taken xor 1);
  return taken;
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
       
    TRACE_InsertCall(trace, IPOINT_BEFORE, AFUNPTR(traceAddr), IARG_FAST_ANALYSIS_CALL, IARG_ADDRINT, TRACE_Address(trace), IARG_END);
    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl))
    {
    	INS ins = BBL_InsTail(bbl); //Last instruction of bbl
     
      if (INS_IsBranchOrCall(ins) || INS_IsRet(ins))
      {

     	 	INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) setCJMP,
     	 		 IARG_CALL_ORDER, CALL_ORDER_FIRST,
     	 		 IARG_FAST_ANALYSIS_CALL,
                 IARG_BRANCH_TAKEN,
                 IARG_END);
     	 	/*
            INS_InsertFillBufferThen(ins, IPOINT_BEFORE, bufId,
 	 	     	                                         IARG_ADDRINT, ((TraceAddr & 0x00ffffff)<<8|BBNumber), offsetof(struct TRACEREF, traceId),
 	 	     	                                       //  IARG_UINT32, TraceCount, offsetof(struct TRACEREF, traceCount),
 	 	     	       	 	     	                     IARG_END);
           */
            INS_InsertFillBufferThen(ins, IPOINT_TAKEN_BRANCH, bufId,
            		                                     IARG_ADDRINT, ((TRACE_Address(trace) & 0x00ffffff)<<8), offsetof(struct TRACEREF, traceId),
 	 	     	                                       //  IARG_UINT32, TraceCount, offsetof(struct TRACEREF, traceCount),
 	 	     	       	 	     	                     IARG_END);

    	/*
   	 	INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) setCJMP,
               IARG_FAST_ANALYSIS_CALL,
               IARG_BRANCH_TAKEN,
               IARG_END);
        INS_InsertFillBuffer(ins, IPOINT_TAKEN_BRANCH, bufId, IARG_ADDRINT, ((TraceAddr & 0x00ffffff)<<8|BBNumber), offsetof(struct TRACEREF, traceId), IARG_END);
        */
      }
    }
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

}


VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    MLOG * mlog = static_cast<MLOG*>(PIN_GetThreadData(mlog_key, tid));
    totalBuffersFilled += mlog->NumBuffersFilled();
    totalElementsProcessed +=  mlog->NumElementsProcessed();
    printf ("totalBuffersFilled %u\ntotalElementsProcessed %14.0f\n", (totalBuffersFilled),
              static_cast<double>(totalElementsProcessed));

    delete mlog;

    PIN_SetThreadData(mlog_key, 0, tid);
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
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    
    // Initialize the memory reference buffer;
    // set up the callback to process the buffer.
    //
    bufId = PIN_DefineTraceBuffer(sizeof(struct TRACEREF), KnobNumPagesInBuffer,
                                  BufferFull, 0);

    if(bufId == BUFFER_ID_INVALID)
    {
        cerr << "Error: could not allocate initial buffer" << endl;
        return 1;
    }

   // Obtain  a key for TLS storage.
    mlog_key = PIN_CreateThreadDataKey(0);

    CODECACHE_AddCacheInitFunction(LimitTraces, 0);

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


