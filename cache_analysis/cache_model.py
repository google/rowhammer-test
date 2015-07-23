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


def assert_eq(x, y):
    if x != y:
        raise AssertionError('%r != %r' % (x, y))


# This class models a single cache set of a cache that uses "Bit-PLRU", as
# described in https://en.wikipedia.org/wiki/Pseudo-LRU.
class CacheBitPLRU(object):

  def __init__(self, num_ways, randomise=True):
    self.mru_bits = [False] * num_ways
    self.addr_to_way = {}
    self.way_to_addr = [None] * num_ways

    if randomise:
      for way in xrange(num_ways):
        self.mru_bits[way] = bool(random.randrange(2))

  def _evict(self):
    for way in xrange(len(self.mru_bits)):
      if not self.mru_bits[way]:
        return way
    raise AssertionError('Invariant broken: all MRU bits set')

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

    # Are all MRU bits set?  If so, reset them to zero.  This restores the
    # invariant: We don't want all the MRU bits to be set.  Otherwise,
    # cache hits will not be recording any useful information -- they won't
    # be marking cache lines are more recently used than others.
    if all(self.mru_bits):
      for i in xrange(len(self.mru_bits)):
        self.mru_bits[i] = False
      # We still want the cache line that we accessed to be marked as
      # recently used, though.
      self.mru_bits[way] = True

    return is_miss

  def mru_state(self):
    return ''.join(str(int(x)) for x in self.mru_bits)


def test():
  cache = CacheBitPLRU(4, randomise=False)
  assert_eq(cache.mru_state(), '0000')

  def check(addr, is_miss, state):
    assert_eq(cache.lookup(addr), is_miss)
    assert_eq(cache.mru_state(), state)

  check(0, True, '1000')
  check(1, True, '1100')
  check(2, True, '1110')
  check(3, True, '0001')


def main():
  test()

  parser = argparse.ArgumentParser()
  parser.add_argument('--order', default='seq')
  parser.add_argument('--show-state', '-s', action='store_true')
  args = parser.parse_args()

  ways = 12
  cache = CacheBitPLRU(ways)

  if args.order == 'seq':
    addr_order = range(ways + 1)
  elif args.order == 'hammer':
    # Try a "rowhammer optimal" ordering of addresses to access.  This
    # should generate cache misses on just two specific addresses on each
    # iteration.
    addr_order = ([100] + range(ways - 1) +
                  [101] + range(ways - 1))
  else:
    parser.error('Unknown "--order" option: %r' % args.order)

  print 'ordering of addresses to access:', addr_order

  for run in xrange(30):
    results = []
    for addr in addr_order:
      results.append(cache.lookup(addr))
      if args.show_state:
        print 'state:', cache.mru_state()
    print 'misses: %s  (total: %i)' % (
        ''.join(str(int(x)) for x in results),
        sum(results))


if __name__ == '__main__':
    main()
