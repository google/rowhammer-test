
# Measuring the DRAM refresh rate by timing memory accesses

It is possible to observe when DRAM refreshes occur just by timing
uncached (`CLFLUSH`'d) memory accesses.  We can use this to deduce
what DRAM refresh rate a machine is configured to use.  This works
from normal unprivileged user processes.

This is useful because it tells us whether a machine uses a 1x or 2x
refresh rate.  Using a 2x refresh rate is a mitigation for the
rowhammer problem.  It is currently the main mitigation that vendors
are applying.  However, vendors don't always document what rowhammer
mitigations they deploy.  This method of measuring the refresh rate
lets us check, without depending on vendors.


## How this works

Every row in a bank of DRAM must be periodically refreshed, and while
it is being refreshed the DRAM cannot respond to memory accesses from
the CPU.  Therefore, if a program repeatedly does uncached accesses to
the same memory location, it will periodically see a delay where the
memory access is slower than usual.

On a machine using the standard 1x refresh rate -- with a 64ms refresh
period, as specified by DDR3 -- we will see a delay every ~7.8us.

On a machine using 2x refresh rate -- with a 32ms refresh period -- we
will see a delay every ~3.9us.

This is because a DRAM module's rows aren't all refreshed in one go;
they are refreshed in 8192 batches.  So, for example, if a DRAM module
has 32768 rows, each refresh operation will refresh 4 rows.  The
standard delay between refresh operations will therefore be 64ms /
8192 = ~7.8us.

See [A DRAM Refresh
Tutorial](http://utaharch.blogspot.com/2013/11/a-dram-refresh-tutorial.html)
from the Utah Arch blog.


## Usage

```
./make.sh
./refresh_timing
```

This produces two outputs:

* A graph of memory access times, displayed in the `graph.html` file.

* Text output showing likely refresh rates.


## Future work

* **Javascript version:** It should be possible to measure the DRAM
  refresh rate from Javascript too, on browsers that provide a
  sufficiently high resolution timer.

  Javascript code can only used normal cached memory accesses --
  `CLFLUSH` is not available in Javascript -- so this would involve
  finding a sequence of memory accesses that consistently generates a
  cache miss.

* **Testing for TRR support:** It should be possible to test whether a
  memory controller supports Target Row Refresh (TRR) and has it
  enabled.  If TRR is enabled, then if we repeatedly activate two
  different rows, we should see a delay when the memory controller
  refreshes their four neighbouring rows.
