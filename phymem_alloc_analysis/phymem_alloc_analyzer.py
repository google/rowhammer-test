import operator

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
        ('row', ExtractBits(phys, 18, 15)),
    ]

    d = dict(fields)
    # The bottom 3 bits of the row number are XOR'd into the bank number.
    d['bank'][0] ^= d['row'][0] & 7
    return fields


def GetResultPfns(log_filename):
    for line in open(log_filename):
        parts = line.strip('\n')[:-1].split(' ');
        yield [int(part, 16) for part in parts]


def Main():
    counter = 0
    cont_count = {}
    for pfns in GetResultPfns('phymem_alloc_results'):
        i1 = 0
        i2 = 0
        while i2 < len(pfns):
            while i2 + 1 < len(pfns) and pfns[i2+1] - pfns[i2] == 1:
                i2 += 1
            size = i2 - i1 + 1
            if size not in cont_count:
                cont_count[size] = 1
            else:
                cont_count[size] += 1
            i2 += 1
            i1 = i2

    total_pages = 0
    for size, count in cont_count.iteritems():
        total_pages += size * count
    cont_percent = {}
    for size, count in cont_count.iteritems():
        cont_percent[size] = 1.0 * size * count / total_pages

    sorted_by_size = sorted(cont_percent.items(), key=operator.itemgetter(0))
    for pair in sorted_by_size:
        size = pair[0]
        percent = pair[1]
        count = cont_count[size]
        size_total = size * count
        print "%10d %10d %10d %10.2f" % (size, count, size_total, percent)


if __name__ == '__main__':
    Main()
