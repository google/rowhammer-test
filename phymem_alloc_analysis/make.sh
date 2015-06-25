#!/bin/bash

set -eu

cflags="-g -Wall -Werror -O2"

gcc $cflags phymem_alloc_profiler.c -o phymem_alloc_profiler
