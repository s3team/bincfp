# bincfp
Efficient Multi-threaded Binary Code Control Flow Profiling Pintool

BinCFP is an efficient multi-threaded binary code control flow profiling tool by taking advantage of pervasive multicore
platforms. BinCFP relies on dynamic binary instrumentation to work with the unmodified binary code. The key of BinCFP is 
a multi-threaded fast buffering scheme that supports processing trace buffers asynchronously. To achieve better performance
gains, we also apply a set of optimizations to reduce control flow profile size and instrumentation overhead. Our design enables the complete dynamic control flow collection for an obfuscated binary execution. Please check our SCAM 2016 paper for more information.

"BinCFP: Efficient Multi-threaded Binary Code Control Flow Profiling", by Jiang Ming and Dinghao Wu. In Proceedings of the 16th IEEE International Working Conference on Source Code Analysis and Manipulation, Engineering Track, Raleigh, NC, October 2-3, 2016.


Usage:

1. Put bincfp folder to pin-2.12/source/tools/.

2. cd pin-2.12/source/tools/bincfp & make  (please check Intel Pin manual to find out how to install Pintool)

3. bincfp folder contains mulitple trace porfiling pintools. To get a listing of the available command line options for each bincfp pintool:
   pin-2.12/pin -t pin-2.12/source/tools/bincfp/obj-ia32/bbdep_threadpool2.so -help -- /bin/ls

4. Common options:

   -num_buffers_per_app_thread <num>  :number of buffers to allocate per application thread,        default   3  
   
   -num_pages_in_buffer <num>         :number of (4096byte) pages allocated in each buffer,         default 64M
   
   -process_buffs <0 or 1>            :specify 0 to disable processing of the buffers,              default   1
   
   -num_processing_threads <num>      :number of internal-tool buffer processing threads to create, default   3
   
   -num_bbs_in_trace <num>            :number of basic blocks per trace,                            default   3
   
   -filter_no_shared_libs             :filter out shared library code
   
5. Example:

   pin-2.12/pin -t pin-2.12/source/tools/bincfp/obj-ia32/bbdep_threadpool2.so -filter_no_shared_libs -num_pages_in_buffer 32768 -num_processing_threads 1 -num_buffers_per_app_thread 2 -- ./gzip readme_10m 
