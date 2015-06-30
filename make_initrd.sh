#!/bin/bash

# Copyright 2015, Google, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# This script builds a bootable Linux "initrd" image which will run
# rowhammer_test as the only process on the system.
#
# See docs/bootable_rowhammer_test.md for how to use this.

set -eu

cflags="-g -Wall -Werror -O2 -static"

mkdir -p out

g++ $cflags rowhammer_test.cc -o out/init
(cd out && echo init | cpio -H newc -o --quiet) \
    | gzip >out/rowhammer_test_initrd.gz
