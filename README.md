
# Program for testing for the DRAM "rowhammer" problem


"Rowhammer" is a problem with recent DRAM modules in which repeatedly
accessing a row of memory can cause bit flips in adjacent rows.  This
repo contains a program for testing for the rowhammer problem which
runs as a normal userland process.

The rowhammer problem is described by:

* Yoongu Kim et al's paper, "[Flipping Bits in Memory Without
  Accessing Them: An Experimental Study of DRAM Disturbance
  Errors](http://users.ece.cmu.edu/~yoonguk/papers/kim-isca14.pdf)"
  (2014).

* Our blog post, "[Exploiting the DRAM rowhammer bug to gain kernel
  privileges](http://googleprojectzero.blogspot.com/2015/03/exploiting-dram-rowhammer-bug-to-gain.html)"
  (2015), on the blog of Google's Project Zero.

How to run the test:

```
./make.sh
./rowhammer_test
```

The test should work on Linux or Mac OS X.  It works on x86 only
(either x86-32 or x86-64), because it relies on x86's `CLFLUSH`
instruction for flushing cache lines.

**Warning #1:** We are providing this code as-is.  You are responsible
for protecting yourself, your property and data, and others from any
risks caused by this code.  This code may cause unexpected and
undesirable behavior to occur on your machine.  This code may not
detect the vulnerability on your machine.

Be careful not to run this test on machines that contain important
data.  On machines that are susceptible to the rowhammer problem, this
test could cause bit flips that crash the machine, or worse, cause bit
flips in data that gets written back to disc.

**Warning #2:** If you find that a computer is susceptible to the
rowhammer problem, you may want to avoid using it as a multi-user
system.  Bit flips caused by row hammering breach the CPU's memory
protection.  On a machine that is susceptible to the rowhammer
problem, one process can corrupt pages used by other processes or by
the kernel.


## Mailing list

We invite people to post results from this test on the following
mailing list:

https://groups.google.com/group/rowhammer-discuss/

This mailing list is intended to be used for:

* Reporting experimental results.
* Discussing avenues of exploitation for rowhammer-induced bit flips.
* Discussing mitigations.
* Any other discussion of the rowhammer problem.


## How the test works

A row hammering attempt involves picking two or more memory locations
and then accessing them, uncached, repeatedly.  If the locations are
in different rows of DRAM but in the same bank, this will cause the
rows to be activated repeatedly.  It is these repeated row activations
that can cause bit flips in adjacent rows.

We use a probabilistic approach for picking memory locations: We can
simply pick random pairs of addresses, and retry repeatedly.  If a
machine has 16 banks of DRAM, there should be a 1/16 chance that the
two addresses chosen map to the same bank.  (For example, some
machines that I've tested contain 2 DRAM modules with 8 banks each.)

This probabilistic approach means that the test doesn't need to know
how the CPU's memory controller maps physical addresses to DRAM row
and column numbers, and it doesn't need to know the physical addresses
of the memory it has allocated.

The test allocates a large block of memory.  It repeatedly picks >2
random addresses within the block, hammers them, and then checks the
block for bit flips.  If it sees a bit flip, it exits.  If it never
sees a bit flip, it will run forever.
