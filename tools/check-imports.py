#!/usr/bin/env python

from __future__ import print_function
import glob
import re
import sys


def do_exist(file_name, lines, imported):
  if not any(not re.match('using \w+::{0};'.format(imported), line) and
             re.search(imported, line) for line in lines):
    print('File "{0}" does not use "{1}"'.format(file_name, imported))
    return False
  return True


def is_valid(file_name):
  with open(file_name) as source_file:
    lines = [line.strip() for line in source_file]

  usings, importeds, line_numbers, valid = [], [], [], True
  for idx, line in enumerate(lines, 1):
    matches = re.search(r'^using (\w+::(\w+));$', line)
    if matches:
      line_numbers.append(idx)
      usings.append(matches.group(1))
      importeds.append(matches.group(2))

  valid = all([do_exist(file_name, lines, imported) for imported in importeds])

  sorted_usings = sorted(usings, key=lambda x: x.lower())
  if sorted_usings != usings:
    print("using statements aren't sorted in '{0}'.".format(file_name))
    for num, actual, expected in zip(line_numbers, usings, sorted_usings):
      if actual != expected:
        print('\tLine {0}: Actual: {1}, Expected: {2}'
            .format(num, actual, expected))
    return False
  else:
    return valid

sys.exit(0 if all(map(is_valid, glob.iglob('src/*.cc'))) else 1)
