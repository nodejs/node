#!/usr/bin/env python3

import argparse
import sys

def wrap(args):
    firstline = True
    while (line := args.input.read(72)):
        if line.endswith('\n'):
            if firstline:
                outstr = f"{line}"
            else:
                outstr = f" {line}"
                firstline = True
        else:
            if firstline:
                outstr = f"{line:<71}*\n"
                firstline = False
            else:
                outstr = f" {line:<70}*\n"
            line = line[-1] + args.input.read(70)
        args.output.write(outstr)
    return 0

def unwrap(args):
    firstline = True
    for line in args.input:
        if len(line) > 80:
            print("Error: input line invalid (longer than 80 characters)", file=sys.stderr)
            return 1
        if not firstline and line[0] != ' ':
            print("Error: continuation line does not start with a blank", file=sys.stderr)
            return 1

        if len(line) > 71 and line[71] == '*':
            if firstline:
                args.output.write(line[:71])
                firstline = False
            else:
                args.output.write(line[1:71])
        else:
            if firstline:
                args.output.write(line)
            else:
                args.output.write(line[1:])
                firstline = True
    return 0

def main():
    parser = argparse.ArgumentParser(description="Wrap sidedeck source to card formats")
    parser.add_argument("-u", "--unwrap", help="Unwrap sidedeck cards to source formats instead", action="store_true")
    parser.add_argument("-i", "--input", help="Input filename, defaults to stdin", default=None)
    parser.add_argument("-o", "--output", help="Output filename, defaults to stdout", default=None)

    args = parser.parse_args()

    with open(args.input, 'r') if args.input else sys.stdin as infile, \
         open(args.output, 'w') if args.output else sys.stdout as outfile:

        args.input = infile
        args.output = outfile

        if args.unwrap:
            return unwrap(args)
        return wrap(args)

if __name__ == '__main__':
    sys.exit(main())
