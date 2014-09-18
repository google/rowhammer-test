#!/bin/bash

set -eu

g++ -g -Wall -Werror -O2 rowhammer_test.cc -o rowhammer_test
