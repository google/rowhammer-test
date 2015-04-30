
# rowhammer_test_ext: Extended version of rowhammer_test


This directory contains an extended version of rowhammer_test which
reports physical memory addresses.

rowhammer_test_ext has the following differences from rowhammer_test:

* It reports the physical addresses of victim locations (memory
  locations where bit flips occur) and aggressor locations (pairs of
  memory locations which cause the bit flips when accessed).

  When rowhammer_test_ext finds that accessing a batch of addresses
  produces a bit flip, the program tries to narrow down which pair of
  addresses in the batch will reproduce the bit flip.

* This version is Linux-specific, because it uses `/proc/self/pagemap`
  to find the physical addresses of pages.

* This version keeps on running when it finds a bit flip, rather than
  exiting.


## How to run the test

```
./make.sh
./rowhammer_test_ext
```

If you want to save the results, you can run:

```
./rowhammer_test_ext 2>&1 | tee -a log_file
```


## Why a separate "extended version"?

rowhammer_test_ext.cc is based on rowhammer_test.cc, but I am keeping
them separate so that rowhammer_test.cc stays as simple and portable
as possible.

rowhammer_test.cc is a very simple demonstration of how to do row
hammering using random address selection, and I don't want to clutter
it with Linux-specific code that might fail to build or run on other
flavours of Unix.


## Output format

When the program finds a pair of aggressor addresses that reproduce a
bit flip, it outputs a line of the following format:

```
RESULT PAIR,addr_agg1,addr_agg2,addr_victim,bit_number,flips_to
```

where:

* `addr_agg1` and `addr_agg2` are the physical addresses of the
  aggressor locations.

* `addr_victim` is the physical address of the 64-bit victim location,
  where the bit flip occurred.

* `bit_number` is the number of the bit that flipped within the 64-bit
  victim location.

  This is useful for checking whether the bit flip can be used for
  particular exploits, such as the PTE-based exploit (described in the
  [blog post](http://googleprojectzero.blogspot.com/2015/03/exploiting-dram-rowhammer-bug-to-gain.html))
  which involves flipping particular bits in a page table entry (PTE).

* `flips_to` is the value that the bit flipped to, either 0 or 1.
  (However, note that the test currently initialises memory to all 1s,
  which means that `flips_to` will be 0.)

Example:

```
RESULT PAIR,0x194d63000,0x194cf8000,0x194d27b30,16,0
```
