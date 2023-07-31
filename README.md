# qfsort
** yet another external file sort but sorts quotefiles**

1. **An outline of the files for the reader**

- ***merge.cpp***
This file has the class MultipassMerge. This class orchestrates the input files to merge
it uses the helper class workscheduler.essentially all it does is give the input to the next
stage of processing. A stage is a node level in the merge tree.   
<br>

- **workscheduler.h**
This class processes workItems in stages till the Work object reduces it.
    $Stage_i($Stage_i-1(W_i-1)$) == done claimed by the Work object
    the work type has the following WorkType and WorkParameter and a WorkMethod
    the WorkMethod is called with a list of WorkItems and WorkParameter.
    Its uses std::async to do this as the actual work takes so long, doing any
    custom threading here is meaningless. This scheduler is reusable for many 
    similar work, like calculate something on a large work set. Will have to check the latest
    c++ executer standard.
    <br>

- **mergeworker.cpp**
This file has the free function mergeQuoteFiles which can be called from any thread
and task with a set of files to merge. It uses an heap to insert and file records, and
a custom heap element comparator. By adapting the heap comparator its possible to
sort files with different kinds of records and comparison keys.
The implementation has a template with the stage as tags to distinguish whether its
processing leaves or internal nodes as the file record(csv lines) are different.
leaf nodes dont have symbol and internal nodes do. 
<br>

- **symqfile.cpp**
This file has a class to hold the symquote files. It uses read/write binary mode I/O
and a line buffer class to extract lines. It uses the helper class LineBuffer to do
this which produces a stream of QuoteRecords. QuoteRecords hold a view of the underlying
line in the buffer and have 2 additional field symbol && timestamp. Both these fields
are represented as a union of string and integer and is set externally by the heap 
comparator. I had intented to parse timestamps to numeric and symbols to a symbolID 
because the universe of symbols is known at start. I didnt have time to complete this.
Using numeric comparison should improve the heap performance.
This class also always allocates nBuffers*bufSize irrespective of filesize.
It would be better to allocate morebuffers keeping bufSize smaller. more 1MB blocks
as the actual I/O operation may take more time to complete with large blocks. I do
have a 5 sec I/O timeout.
<br>

- **iohandler.cpp**
This class handles the actual I/O. It creates reader/writer threads and assigns read/writes
to them keeping the file descriptor in the same thread. Its interface registers descriptors
as i just wanted to offload the I/O strategy from the symqfile which has an obvious design
flaw. If the file descriptor is closed with outstanding operations and quickly reopened in
the process. This can be easily avoided by doing the entire life cycle management in the
I/O handler i mean open/close. So registerDescriptor should change to registerFile and its
easy to make the actual operations lock free which it currently does not.
<br>

- **cmdline.cpp**
well i started with boost program_options might have even used it if it was header only.
I kept the interface. This code does not support non-default options, its trivial to do do.
Also empty strings as defaults does not work. Its because it has only one map in which it
inserts default values if not specified. So extraction fails. Its easy to avoid this by
having a separate default values of std::any.
<br>

- **util.cpp**
file directory iterator, a filter to collect input symquote files QuoteFileFilter, a very
simple lock based single-producer consumer queue PCQueueT and some template helper metafunctions
<br>

2. **Design goals**
S--> total input file size
N--> number of input files 
This program is very I/O intensive and the actual merge is not very thread friendly.
the minimal total I/O with infinite memory and number of open files{this limit is not so real maybe for 20,000 symbols on 64 bit systems) can be done by writing 2*S bytes.
<br>
The limiting the number of total files to k {this limit could also be dynamically figured
by limiting memory usage} --you have a tree of $log{_k}{n}$ height where each level has the
same I/O (S bytes read + S bytes written) with a total I/O of  $log{_k}{n}*O(S). Threading only helps on the leaf level if you can keep the height of the tree the same. Even though there are more files in the leaf level there sum should be less than the total open file limit. So the adjustment of threads should be dynamic which currently this program does not to do. You can control this by setting minFilesPerThread but ideally it should be automatic.
<br>
So the merge class (the manager) needs to have  an assignment strategy. Currently all it
does it tries to divide files by size.
<br>
So the design goal is primarily to reduce the total I/O. The secondary goal is to
speed up the I/O so that the mergeWorker never blocks. Offloading the read/write to a separate subsystem is a good idea, but need to play with various implementation async i/o and mem-mapped I/O where one allocates a large address range and maps file pages to avoid system
call overhead.

3. **whats  missing**
I have tested this with what i could and can primarily on windows and did run on linux
I did not have time to checkin testcases or more testing as generating all this data
takes time. I did download TAQ file from nyse and reformatted it but i ran out of space.
So i have tested with some 1000 files. There could be some assert kicking in for some edge
case as i have not played around with the program enough or written more formal test cases.
Thats with reference to the code and testing.
<br>
Whats really missing is any sort of performance data. The i/o handler does not have
request processing time stats, symqfile does not have waiting for i/o stats. One needs
that to tune 
