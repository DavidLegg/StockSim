from glob import glob
import os, sys, re

USAGE="""sanitize_resources [PATTERN]...
    Processes files matching PATTERN(s), scrubbing timestamps to be ints, and prices to be floats.
"""

# Unix Timestamp,Date,Symbol,Open,High,Low,Close,Volume
INT_COL_NAMES   = ['Unix Timestamp']
FLOAT_COL_NAMES = ['Open', 'High', 'Low', 'Close', 'Volume']

def main():
    try:
        patterns = sys.argv[1:]
        assert len(patterns) > 0, 'Must provide at least 1 pattern'
        if patterns[0].lower() in {'-h', '--help'}:
            print(USAGE)
            return
    except Exception as e:
        print(e)
        print(USAGE)
        return

    print('Finding files...')
    fns = {fn for p in patterns for fn in glob(p)}
    total = len(fns)
    print('Found {} files. Processing...{:5.1f}%'.format(total, 0.0), end='', flush=True)
    for i,filename in enumerate(fns):
        output_filename = filename + '.in-progress'
        process(filename, output_filename)
        os.rename(output_filename, filename)
        print("\b\b\b\b\b\b{:5.1f}%".format(100.0 * i / total), end='', flush=True)
    print(" - Done.")

def process(in_file, out_file):
    intCols   = []
    floatCols = []
    with open(in_file) as f_in:
        with open(out_file, 'w') as f_out:
            parts = next(f_in).strip().split(',')
            for icn in INT_COL_NAMES:
                try:
                    intCols.append(parts.index(icn))
                except (ValueError, IndexError):
                    pass
            for fcn in FLOAT_COL_NAMES:
                try:
                    floatCols.append(parts.index(fcn))
                except (ValueError, IndexError):
                    pass
            print(','.join(parts), file=f_out)
            for line in f_in:
                parts = line.strip().split(',')
                for i in intCols:
                    try:
                        parts[i] = str(int(float(parts[i])))
                    except (ValueError, IndexError):
                        pass
                for i in floatCols:
                    try:
                        parts[i] = str(float(parts[i]))
                    except (ValueError, IndexError):
                        pass
                print(','.join(parts), file=f_out)

if __name__ == '__main__':
    main()
