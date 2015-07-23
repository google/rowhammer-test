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

import argparse
import random


# This class models a single cache set of a cache that uses "Bit-PLRU", as
# described in https://en.wikipedia.org/wiki/Pseudo-LRU.
class CacheBitPLRU(object):

  def __init__(self, num_ways):
    self.mru_bits = [False] * num_ways
    self.addr_to_way = {}
    self.way_to_addr = [None] * num_ways

    for way in xrange(num_ways):
      self.mru_bits[way] = bool(random.randrange(2))

  def _evict(self):
    for way in xrange(len(self.mru_bits)):
      if not self.mru_bits[way]:
        return way
    # All MRU bits were set, so reset them all to zero.
    for way in xrange(len(self.mru_bits)):
      self.mru_bits[way] = False
    return 0

  def lookup(self, addr):
    way = self.addr_to_way.get(addr)
    is_miss = way is None
    if is_miss:
      way = self._evict()

      # Evict old address.
      old_addr = self.way_to_addr[way]
      if old_addr is not None:
        del self.addr_to_way[old_addr]

      self.addr_to_way[addr] = way
      self.way_to_addr[way] = addr

    # Mark as recently used.
    self.mru_bits[way] = True
    return is_miss

  def mru_state(self):
    return ''.join(str(int(x)) for x in self.mru_bits)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--show-state', '-s', action='store_true')
  args = parser.parse_args()

  ways = 12
  cache = CacheBitPLRU(ways)

  # Try a "rowhammer optimal" ordering of addresses to access.  This should
  # generate cache misses on just two specific addresses on each iteration.
  addr_order = ([100] + range(ways - 1) +
                [101] + range(ways - 1))
  print 'ordering of addresses to access:', addr_order

  for run in xrange(30):
    results = []
    for addr in addr_order:
      results.append(cache.lookup(addr))
      if args.show_state:
        print 'state:', cache.mru_state()
    print 'misses:', ''.join(str(int(x)) for x in results)


if __name__ == '__main__':
    main()
