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


# This script examines the physical aggressor/victim addresses
# outputted by rowhammer_test_ext, and it checks whether these
# addresses match a model of how physical addresses are mapped to DRAM
# row/bank/column numbers.
#
# This script explains the resulting addresses from a laptop with a
# Sandy Bridge CPU which has 2 * 4GB SO-DIMMs.
#
# For this laptop, decode-dimms reports:
#   Size                                            4096 MB
#   Banks x Rows x Columns x Bits                   8 x 15 x 10 x 64
#   Ranks                                           2


def GetResultAddrs(log_filename):
  for line in open(log_filename):
    if line.startswith('RESULT '):
      parts = line[len('RESULT '):].strip('\n').split(',')
      if parts[0] == 'PAIR':
        yield [int(part, 16) for part in parts[1:]]


def FormatBits(val, bits):
  got = []
  for bit in xrange(bits - 1, -1, -1):
    got.append(str((val >> bit) & 1))
  return ''.join(got)


def ExtractBits(val, offset_in_bits, size_in_bits):
  return [(val >> offset_in_bits) & ((1 << size_in_bits) - 1),
          size_in_bits]


def Convert(phys):
  fields = [
    ('col_lo', ExtractBits(phys, 0, 6)),
    ('channel', ExtractBits(phys, 6, 1)),
    ('col_hi', ExtractBits(phys, 7, 7)),
    ('bank', ExtractBits(phys, 14, 3)),
    ('rank', ExtractBits(phys, 17, 1)),
    ('row', ExtractBits(phys, 18, 14)),
    ]

  d = dict(fields)
  # The bottom 3 bits of the row number are XOR'd into the bank number.
  d['bank'][0] ^= d['row'][0] & 7
  return fields


def Format(fields):
  return ' '.join('%s=%s' % (name, FormatBits(val, size))
                  for name, (val, size) in reversed(fields))


def Main():
  count = 0
  count_fits = 0
  for addrs in GetResultAddrs('bitflip_addrs'):
    aggs = addrs[0:2]
    victim = addrs[2]
    # Sort aggressor addresses by closeness to victim.  We assume
    # the closest one is the one that causes the victim's bit flip.
    aggs.sort(key=lambda agg: abs(victim - agg))

    def FormatAddr(name, val):
      fmt = Format(Convert(val))
      print '\taddr=0x%09x -> %s (%s)' % (val, fmt, name)

    print 'result:'
    print '\tdiff=%x' % (victim - aggs[0])
    FormatAddr('victim', victim)
    FormatAddr('aggressor1', aggs[0])
    FormatAddr('aggressor2', aggs[1])

    # Test hypotheses.
    agg1_dict = dict(Convert(aggs[0]))
    agg2_dict = dict(Convert(aggs[1]))
    victim_dict = dict(Convert(victim))
    row_diff = abs(agg1_dict['row'][0] - victim_dict['row'][0])
    fits = (agg1_dict['bank'] == victim_dict['bank'] and
            agg2_dict['bank'] == victim_dict['bank'] and
            row_diff in (1, -1))
    print '\t' + 'fits=%s' % fits
    if agg1_dict['row'][0] & 2 != victim_dict['row'][0] & 2:
      print '\t' + 'unusual?'

    count += 1
    if fits:
      count_fits += 1

  print "\nSummary: of %i results, %i fit and %i don't fit" % (
      count, count_fits, count - count_fits)


if __name__ == '__main__':
  Main()
