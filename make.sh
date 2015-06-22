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

set -eu

cflags="-g -Wall -Werror -O2"

g++ $cflags rowhammer_test.cc -o rowhammer_test

if [ "$(uname)" = Linux ]; then
  g++ $cflags -std=c++11 double_sided_rowhammer.cc -o double_sided_rowhammer
fi
