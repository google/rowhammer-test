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


def GetResultPfns(log_filename):
  for line in open(log_filename):
    parts = line.strip('\n')[:-1].split(' ');
    yield [int(part, 16) for part in parts]


def Main():
  cont_count = {}
  for pfns in GetResultPfns('physmem_alloc_results'):
    i1 = 0
    i2 = 0
    while i2 < len(pfns):
      while i2 + 1 < len(pfns) and pfns[i2+1] - pfns[i2] == 1:
        i2 += 1
      size = i2 - i1 + 1
      cont_count.setdefault(size, 0)
      cont_count[size] += 1
      i2 += 1
      i1 = i2

  total_pages = 0
  for size, count in cont_count.iteritems():
    total_pages += size * count

  sorted_by_size = sorted(cont_count.iteritems())
  for size, count in sorted_by_size:
    size_total = size * count
    fraction = 1.0 * size_total / total_pages
    print '%10d %10d %10d %10.2f' % (size, count, size_total, fraction)

if __name__ == '__main__':
  Main()
