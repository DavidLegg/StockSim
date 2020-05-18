from glob import glob
import os, sys, re

USAGE = """Usage: mk_symbols_file [PATTERN]...
Make resources/symbols.txt, using the files matching given PATTERN(s).

  Each PATTERN must contain exactly one "*" wildcard,
  for the location in the path of the symbol.
  Earlier patterns have precedence over later ones.

Examples:
  python3 mk_symbols_file.py resources/*_daily_bars.csv
"""

CORRUPTED_FILENAME = 'resources/corrupted_files.txt'
OUTPUT_FILENAME    = 'resources/symbols.txt'
TIME_COL_NAME      = 'Unix Timestamp'
PRICE_COL_NAME     = 'Close'
GARBAGE_THRESH     = 5

def main():
    try:
        patterns = sys.argv[1:]
        assert len(patterns) > 0, 'Must provide at least 1 pattern'
        if patterns[0].lower() in {'-h', '--help'}:
            print(USAGE)
            return
        assert all(p.count('*') == 1 for p in patterns), 'Every pattern must have exactly 1 "*" wildcard'
    except Exception as e:
        print(e)
        print(USAGE)
        return

    print('Finding files...')
    # Get a dictionary of pattern -> matching filenames
    fns = {p:glob(p) for p in patterns}
    print('Extracting symbol list...')
    # Flatten and filter list of symbols, extracted from filenames using regex
    symbols = {re.fullmatch(p.replace('*', '(.*)'), x).group(1) for p,xs in fns.items() for x in xs}
    with open(CORRUPTED_FILENAME) as f:
        symbols.difference_update({line.strip() for line in f})
    total = len(symbols)
    print('Found {} symbols. Processing for start & end times...'.format(total))
    with open(OUTPUT_FILENAME, "w") as f_out:
        print('Symbol,Start Time,End Time', file=f_out)
        for i,sym in enumerate(symbols):
            for p in patterns:
                fn = p.replace('*', sym)
                start,end = findStartEnd(fn)
                if start is not None and end is not None:
                    break
            if start is None or end is None:
                print('Error reading data files for {}'.format(sym))
            else:
                print('{},{},{}'.format(sym, start, end), file=f_out)
            print("  Progress -{:5.1f}%".format(100.0 * i / total))
    print('Made symbols file "{}" successfully.'.format(OUTPUT_FILENAME))

def findStartEnd(fn):
    try:
        timeCol  = None
        priceCol = None
        start    = None
        end      = None
        price    = None
        with open(fn) as f:
            while timeCol is None or priceCol is None:
                parts = next(f).strip().split(',')
                try:
                    timeCol = parts.index(TIME_COL_NAME)
                except ValueError:
                    pass
                try:
                    priceCol = parts.index(PRICE_COL_NAME)
                except ValueError:
                    pass
            while start is None:
                try:
                    start = int( next(f).strip().split(',')[timeCol] )
                    if start > 10000000000:
                        start = start // 1000
                except (ValueError, IndexError):
                    pass
            for line in f:
                parts = line.strip().split(',')
                try:
                    end = int( parts[timeCol] )
                    if end > 10000000000:
                        end = end // 1000
                except (ValueError, IndexError):
                    pass
                try:
                    newPrice = int( parts[priceCol] )
                    if price is not None and (newPrice > GARBAGE_THRESH*price or newPrice*GARBAGE_THRESH < price):
                        # Data is bad, return none
                        return None,None
                    price = newPrice
                except (ValueError, IndexError):
                    pass
        return start,end
    except KeyboardInterrupt:
        exit(1)
    except:
        return None,None

if __name__ == '__main__':
    main()
