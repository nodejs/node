#!/usr/bin/env python

import argparse
import sys

def wrap(args):
  l = args.input.readline(72)
  firstline = True
  while l:
    if l[-1] == '\n':
      if firstline:
        outstr = "{}".format(l)
      else:
        outstr = " {}".format(l)
        firstline = True
      l = args.input.readline(72)
    else:
      if firstline:
        outstr = "{:<71}*\n".format(l[:-1])
        firstline = False
      else:
        outstr = " {:<70}*\n".format(l[:-1])
      l = l[-1] + args.input.readline(70)
    args.output.write(outstr)

  return 0

def unwrap(args):
  l = args.input.readline()
  firstline = True
  while l:
    if len(l) > 80:
      print("Error: input line invalid (longer than 80 characters)", file=sys.stderr)
      return 1
    if not firstline and l[0] != ' ':
      print("Error: continuation line not start with blank", file=sys.stderr)
      return 1

    if len(l) > 71 and l[71] == '*':
      if firstline:
        args.output.write(l[:71])
        firstline = False
      else:
        args.output.write(l[1:71])
    else:
      if firstline:
        args.output.write(l)
      else:
        args.output.write(l[1:])
        firstline = True
    l = args.input.readline()
  return 0

def Main():
  parser = argparse.ArgumentParser(description="Wrap sidedeck source to card formats")
  parser.add_argument("-u", "--unwrap",
      help="Unwrap sidedeck cards to source formats instead", action="store_true",
      default=False)
  parser.add_argument("-i", "--input", help="input filename, default to stdin",
      action="store", default=None)
  parser.add_argument("-o", "--output", help="output filename, default to stdout",
      action="store", default=None)

  args = parser.parse_args()

  if args.input is None:
    args.input = sys.stdin
  else:
    args.input = open(args.input, 'r')

  if args.output is None:
    args.output = sys.stdout
  else:
    args.output = open(args.output, 'w')

  if args.unwrap:
    return unwrap(args)

  return wrap(args)

if __name__ == '__main__':
  sys.exit(Main())
