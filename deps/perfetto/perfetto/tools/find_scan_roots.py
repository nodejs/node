#!/usr/bin/env python
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Takes output of
# adb shell 'find /data -print0 | xargs -0 ls -ldZ' | awk '{print $5 " " $9}'
# in standard input and generates list of directories we need to scan to cover
# the labels given on the command line.

import sys
import argparse


class Node(object):

  def __init__(self, name, label=None):
    self.name = name
    self.label = label
    self.marked = False
    self.children = {}

  def Find(self, components):
    if not components:
      return self

    child = components[0]
    if child in self.children:
      return self.children[child].Find(components[1:])

    n = Node(child)
    self.children[child] = n
    return n

  def __iter__(self):
    for child in self.children.itervalues():
      yield self.name + '/' + child.name, child
      for p, ch in child:
        yield self.name + '/' + p, ch

  def Mark(self, labels):
    # Either incorrect label or already marked, we do not need to scan
    # this path.
    if self.marked or self.label not in labels:
      return False

    self.marked = True

    for child in self.children.itervalues():
      child.Mark(labels)

    return True


def BuildTree(stream=sys.stdin):
  root = Node("")

  for line in stream:
    line = line.strip()
    if not line:
      continue

    label, path = line.split(' ', 1)
    #  u:object_r:system_data_file:s0  -> system_data_file.
    sanitized_label = label.split(':')[2]
    # Strip leading slash.
    components = path[1:].split('/')

    n = root.Find(components)
    n.label = sanitized_label
  return root


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'labels', metavar='L', type=str, nargs='+', help='labels we want to find')
  args = parser.parse_args()
  root = BuildTree()
  for fullpath, elem in root:
    if elem.Mark(args.labels):
      print fullpath


if __name__ == '__main__':
  main()
