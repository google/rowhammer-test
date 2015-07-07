
# physmem_alloc_analysis: Physical memory allocation analysis

This directory contains code to profile physical memory allocation
for mmap, and analyze how contiguous the assigned physical frames are.
This analysis is useful for evaluating the feasibility of double-sided
rowhammering when there is no permission to access `/proc/self/pagemap`. 

## How to run the program

To compile and run the profiler:

```
./make.sh
./physmem_alloc_profiler [-a alloc_size] [-s sleep_sec]
```

The profiler mmaps a chunk of memory of `alloc_size` (pages) each time,
computes the assigned physical frames using `/proc/pid/pagemap` interface,
and munmaps it. It then sleeps `sleep_sec` (seconds) before the next iteration. 
The program loops infinitely so manual Control-C is needed to stop it. 
A file named `physmem_alloc_results` is generated to record the assigned 
physical frame numbers.

To analyze the results, run:

```
./analyze.sh
```

This takes `physmem_alloc_results` as input and produces another file: 
`contiguous_results`. As output, it shows the content of `contiguous_results`
and invokes gnuplot for plotting as well.

## Output format

For `physmem_alloc_results`, each line contains information for each mmap
allocation. The assigned physical frame numbers of each page are separated 
by space.

For `contiguous_results` (which is also shown as output for analyze.sh),
each line is of the following format:

```
size    count   size_total    fraction
```

where:

* `size` is the size (in number of pages) of virtual memory area that 
  assigned with contiguous physical frames.

* `count` is the number of times such virtual memory area appears.

* `size_total` = `size` * `count`, i.e., it is the total size 
  (in number of pages) for such memory area.

* `fraction` = `size_total` / `alloc_total`, where `alloc_total` is
  the sum of `size_total` of all rows. Therefore, `fraction` means
  out of all virtual pages, what fraction of them are inside an area
  of `size` pages whose underlying physical memory frames are contiguous.

