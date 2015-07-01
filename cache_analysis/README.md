
# Analysis of the L3 cache's address mapping

This directory contains a program which picks sets of memory locations
that map to the same L3 cache set on 2-core Sandy Bridge CPUs.

It tests whether the locations really do map to the same cache set by
timing accesses to them and outputting a CSV file of times that can be
graphed.

For more explanation, see:
http://lackingrhoticity.blogspot.com/2015/04/l3-cache-mapping-on-sandy-bridge-cpus.html

The following usage generates the data for the graphs in the blog post:

```
./make.sh
./cache_test_physaddr access_time_graph
```
