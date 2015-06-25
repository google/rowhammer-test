#!/bin/bash

set -eu

python ./phymem_alloc_analyzer.py > contiguous_results

gnuplot gnuplot_script -p

cat contiguous_results
