In this design assignment, you will implement and evaluate a number of
cache replacement policies.

PART A: TWO-LEVEL INCLUSIVE CACHE HIERARCHY [50 POINTS]
--------------------------------------------------------

Implement a two-level inclusive cache hierarchy. Since we are interested only
in counting hits and misses in a cache, you will need to model the tags and state bits only.
There is no need to model the cache blocks holding data values. Modify the addrtrace.cpp
pin tool to collect a trace of addresses touched. In addition to the addresses, you
will also have to collect the size of each memory operand to find out how many bytes
are touched starting from the address of the operand. From the address and the size,
you will have to figure out how many different cache blocks should be accessed for an
operand. For example, with a 64-byte block size, if a memory operand has address 122 and
size 8 bytes, the access to this memory operand touches two cache blocks. To complete
this access, you will have to touch two cache blocks. The collected trace should be
passed through a two-level inclusive cache hierarchy with L1 and L2 caches. You need
to model only the data accesses and therefore, there is no need to model instruction
caches. Also, assume that there is no dirty state in the cache block and therefore,
the reads and writes can be treated similarly. Simulate the following configuration.

L1 cache: 64 KB, 8-way set associative, 64-byte block size, LRU replacement policy
L2 cache: 1 MB, 16-way set associative, 64-byte block size, LRU replacement policy

Collect the following statistics.
(i) Number of accesses to L1 cache and L2 cache.
(ii) Number of L1 cache misses and L2 cache misses.
(iii) Number of L2 cache blocks that receive no hit. These blocks are filled into the
L2 cache and evicted before experiencing any hit. Such blocks are referred to as dead-on-fill
or dead-on-arrival. Represent this number as a percentage of the number of blocks filled
into the L2 cache.
(iv) Number of L2 cache blocks that receive at least two hits before getting evicted.
Represent this number as a percentage of number of L2 cache blocks that experience at least
one hit. If the number of L2 cache blocks that experience at least one hit is zero, show this
percentage as zero.

PART B: SRRIP POLICY FOR THE L2 CACHE [30 POINTS]
--------------------------------------------------

Leave the L1 cache unchanged as implemented above. We will study a different policy for the L2
cache in this part. Ensure that your implementation continues to enforce inclusion. There are
two drawbacks of the LRU policy: (i) a block after experiencing its last hit will have to climb
down from the MRU position all the way to the LRU position before getting evicted, (ii) a dead-on-fill
block needs to climb down from the MRU position all the way to the LRU position before getting evicted.
Both of these can take a significant amount of time depending on the associativity of the cache
and the activities within the cache set. The upshot is that useless blocks occupy space in the cache.
In this part, we will modify LRU by attaching a two-bit age with each block so that the age of a
block can go from the smallest (zero) to the largest (three) in just three steps independent of the associativity
of the cache. In the following I discuss how the ages are maintained.

1. When a block is filled into the L2 cache, it is assigned an age of two.
2. When a block receives a hit, its age is updated to zero.
3. The replacement algorithm evicts a block at age three. If there is no such block in the set, the ages of
all blocks in the set are incremented until there is at least one block with age three. Note that the age of
a block is never incremented beyond three. If there are multiple blocks with age three in the set, the block
with the lowest way id among these is evicted.

This policy is referred to as the static re-reference interval prediction (SRRIP). The aforementioned ages
are referred to as re-reference prediction value (RRPV). Notice that there is an implicit prediction of
the next re-reference interval. When a new block is filled, its next re-reference interval is predicted
to be of intermediate length and therefore, the RRPV is two. When a block experiences a hit, it is assumed
that it will soon receive another hit and as a result, RRPV is made zero. When a block's RRPV becomes three,
it is assumed that the next re-reference to the block is in the distant future and as a result, it is chosen
as a victim. These decisions are influenced by the statistics (iii) and (iv) collected in PART A.

Your job is to implement this policy in the L2 cache and count the number of L1 and L2 cache misses.

PART C: NRU POLICY FOR THE L2 CACHE [20 POINTS]
------------------------------------------------

If there is a significant space constraint in the implementation of the replacement policy of the L2 cache,
one meaningful policy is the NRU (not recently used) policy. It requires just one bit per block. Let us refer
to this bit as the reference (REF) bit. The REF bit is managed as follows.

1. When a block is filled into the L2 cache, its REF bit is set to one.
2. When a block receives a hit, its REF bit is set to one.
3. If in any of the aforementioned cases, all blocks in the cache set have REF bits set, the REF bits of all blocks
in that set except the most recently accessed block are reset to zero.
4. The replacement algorithm evicts a block with REF bit zero. If there are multiple blocks with REF bit zero,
the block with the lowest way id among these is evicted. Notice that it is guaranteed to be one of the not recently
used blocks because the blocks with REF bits set to one are more recently used than the ones with REF bit zero.

Your job is to implement this policy in the L2 cache and count the number of L1 and L2 cache misses.

TIPS AND TEST PROGRAM
----------------------

Unlike branch predictors, it is easy to verify the correctness of the implementation of your cache by running your
pin tool on simple programs. I have posted one such program on the course page alongside HW4.txt. You need to vary
the SIZE parameter (keep it a power of two), hand-count the number of accesses and misses (that is easy), and verify
that those counts closely match when you run your pin tool on this program (the match will not be exact due to codes
other than the main function that will also run). Compile the test program as follows.

gcc -static testprog_hw4.c -o testprog_hw4

WHAT TO SUBMIT
---------------

Following benchmark applications from the SPEC 2006 suite should be used for evaluation.

1.  400.perlbench
2.  401.bzip2
3.  403.gcc
4.  429.mcf
5.  450.soplex
6.  456.hmmer
7.  471.omnetpp
8.  483.xalancbmk

Use the exactly same benchmark application setup (i.e., number of instructions
to fast-forward and number of instructions to be used for evaluation) as in the
first homework, except that your work should be put in HW4 directory (as opposed
to HW1 directory). Write your PIN tool so that you can simulate all the policies
and collect all the results in one execution of an instrumented binary. This may
require simultaneously simulating multiple different L1 and L2 caches. This would 
greatly reduce the time to complete this homework because you will fast-forward only 
once for each application.

Prepare a table showing the required statistics for each application. If you notice different
L1 cache miss counts for different L2 cache policies, try to explain that.
Prepare a PDF document with the table and brief explanations for the results. Put 
the document in your HW4 directory. Archive your HW4 directory after removing all .o and .so files:

zip -r HW4_MYROLLNO.zip HW4/

Please note that we will not accept anything other than a .zip file. Replace
MYROLLNO by your roll number. Send HW4_MYROLLNO.zip to cs422autumn2023@gmail.com
with subject line [CS422 HW4].
