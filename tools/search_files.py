#!/usr/bin/env python

"""
This is a utility for recursively searching files under
a specified directory
"""

import argparse
import utils

def main():
  parser = argparse.ArgumentParser(
    description='Search files with a specific extension under a directory',
    fromfile_prefix_chars='@'
  )
  parser.add_argument('--ext', required=True, help='extension to search for')
  parser.add_argument('directory', help='input directory')
  options = parser.parse_args()
  print('\n'.join(utils.SearchFiles(options.directory, options.ext)))

if __name__ == "__main__":
  main()
