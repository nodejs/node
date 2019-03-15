#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import sys


action = sys.argv[1]

if action in ["help", "-h", "--help"] or len(sys.argv) != 3:
  print("Usage: %s <action> <inputfile>, where action can be: \n"
        "help    Print this message\n"
        "plain   Print ASCII tree to stdout\n"
        "dot     Print dot file to stdout\n"
        "count   Count most frequent transition reasons\n" % sys.argv[0])
  sys.exit(0)


filename = sys.argv[2]
maps = {}
root_maps = []
transitions = {}
annotations = {}


class Map(object):

  def __init__(self, pointer, origin):
    self.pointer = pointer
    self.origin = origin

  def __str__(self):
    return "%s (%s)" % (self.pointer, self.origin)


class Transition(object):

  def __init__(self, from_map, to_map, reason):
    self.from_map = from_map
    self.to_map = to_map
    self.reason = reason


def RegisterNewMap(raw_map):
  if raw_map in annotations:
    annotations[raw_map] += 1
  else:
    annotations[raw_map] = 0
  return AnnotateExistingMap(raw_map)


def AnnotateExistingMap(raw_map):
  return "%s_%d" % (raw_map, annotations[raw_map])


def AddMap(pointer, origin):
  pointer = RegisterNewMap(pointer)
  maps[pointer] = Map(pointer, origin)
  return pointer


def AddTransition(from_map, to_map, reason):
  from_map = AnnotateExistingMap(from_map)
  to_map = AnnotateExistingMap(to_map)
  if from_map not in transitions:
    transitions[from_map] = {}
  targets = transitions[from_map]
  if to_map in targets:
    # Some events get printed twice, that's OK. In some cases, ignore the
    # second output...
    old_reason = targets[to_map].reason
    if old_reason.startswith("ReplaceDescriptors"):
      return
    # ...and in others use it for additional detail.
    if reason in []:
      targets[to_map].reason = reason
      return
    # Unexpected duplicate events? Warn.
    print("// warning: already have a transition from %s to %s, reason: %s" %
            (from_map, to_map, targets[to_map].reason))
    return
  targets[to_map] = Transition(from_map, to_map, reason)


with open(filename, "r") as f:
  last_to_map = ""
  for line in f:
    if not line.startswith("[TraceMaps: "): continue
    words = line.split(" ")
    event = words[1]
    if event == "InitialMap":
      assert words[2] == "map="
      assert words[4] == "SFI="
      new_map = AddMap(words[3], "SFI#%s" % words[5])
      root_maps.append(new_map)
      continue
    if words[2] == "from=" and words[4] == "to=":
      from_map = words[3]
      to_map = words[5]
      if from_map not in annotations:
        print("// warning: unknown from_map %s" % from_map)
        new_map = AddMap(from_map, "<unknown>")
        root_maps.append(new_map)
      if to_map != last_to_map:
        AddMap(to_map, "<transition> (%s)" % event)
      last_to_map = to_map
      if event in ["Transition", "NoTransition"]:
        assert words[6] == "name=", line
        reason = "%s: %s" % (event, words[7])
      elif event in ["Normalize", "ReplaceDescriptors", "SlowToFast"]:
        assert words[6] == "reason=", line
        reason = "%s: %s" % (event, words[7])
        if words[8].strip() != "]":
          reason = "%s_%s" % (reason, words[8])
      else:
        reason = event
      AddTransition(from_map, to_map, reason)
      continue


def PlainPrint(m, indent, label):
  print("%s%s (%s)" % (indent, m, label))
  if m in transitions:
    for t in transitions[m]:
      PlainPrint(t, indent + "  ", transitions[m][t].reason)


def CountTransitions(m):
  if m not in transitions: return 0
  return len(transitions[m])


def DotPrint(m, label):
  print("m%s [label=\"%s\"]" % (m[2:], label))
  if m in transitions:
    for t in transitions[m]:
      # GraphViz doesn't like node labels looking like numbers, so use
      # "m..." instead of "0x...".
      print("m%s -> m%s" % (m[2:], t[2:]))
      reason = transitions[m][t].reason
      reason = reason.replace("\\", "BACKSLASH")
      reason = reason.replace("\"", "\\\"")
      DotPrint(t, reason)


if action == "plain":
  root_maps = sorted(root_maps, key=CountTransitions, reverse=True)
  for m in root_maps:
    PlainPrint(m, "", maps[m].origin)

elif action == "dot":
  print("digraph g {")
  for m in root_maps:
    DotPrint(m, maps[m].origin)
  print("}")

elif action == "count":
  reasons = {}
  for s in transitions:
    for t in transitions[s]:
      reason = transitions[s][t].reason
      if reason not in reasons:
        reasons[reason] = 1
      else:
        reasons[reason] += 1
  reasons_list = []
  for r in reasons:
    reasons_list.append("%8d %s" % (reasons[r], r))
  reasons_list.sort(reverse=True)
  for r in reasons_list[:20]:
    print(r)
