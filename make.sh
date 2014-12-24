#!/bin/bash

set -eu

gcc -g -Wall -Werror -O2 rowhammer_test.c -o rowhammer_test
