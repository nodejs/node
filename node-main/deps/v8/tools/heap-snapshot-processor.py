#!/usr/bin/python3
# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys


# Implement this to process the data.
def process_nodes(nodes):
  pass


class Node:

  def __init__(self, id, type, name):
    self.id = id

    assert isinstance(type, str)
    self.type = type

    assert isinstance(name, str)
    self.name = name

    self.edges_from = []
    self.edges_to = []


class Edge:

  def __init__(self, type, from_node, name, to_node):
    assert isinstance(type, str)
    self.type = type

    assert isinstance(from_node, Node)
    self.from_node = from_node

    assert isinstance(name, str)
    self.name = name

    assert isinstance(to_node, Node)
    self.to_node = to_node


def main():
  if len(sys.argv) != 2:
    print("Usage: python3 heap-snapshot-processor.py snapshot.heapsnapshot")
    exit(1)

  f = open(sys.argv[1])
  data = json.load(f)

  # Documentation of the format (caveat: documentation for name_or_index is
  # wrong):
  # https://learn.microsoft.com/en-us/microsoft-edge/devtools-guide-chromium/memory-problems/heap-snapshot-schema
  snapshot = data['snapshot']
  meta = snapshot['meta']

  node_fields = meta['node_fields']
  node_field_count = len(node_fields)
  node_type_ix = node_fields.index('type')
  node_name_ix = node_fields.index('name')
  node_id_ix = node_fields.index('id')
  node_edge_count_ix = node_fields.index('edge_count')

  node_types = meta['node_types']
  node_type_strings = node_types[node_type_ix]

  edge_fields = meta['edge_fields']
  edge_field_count = len(edge_fields)
  edge_type_ix = edge_fields.index('type')
  edge_name_or_index_ix = edge_fields.index('name_or_index')
  edge_to_node_ix = edge_fields.index('to_node')

  edge_types = meta['edge_types']
  edge_type_strings = edge_types[edge_type_ix]

  nodes = data['nodes']
  edges = data['edges']
  strings = data['strings']

  node_objects = dict()

  for node_ix in range(0, len(nodes), node_field_count):
    node_data = nodes[node_ix:(node_ix + node_field_count)]
    node_type = node_type_strings[node_data[node_type_ix]]
    node_name = strings[node_data[node_name_ix]]
    node_id = node_data[node_id_ix]
    node = Node(node_id, node_type, node_name)
    node_objects[node_id] = node

  edge_ix = 0
  for node_ix in range(0, len(nodes), node_field_count):
    node_data = nodes[node_ix:(node_ix + node_field_count)]
    edge_count = node_data[node_edge_count_ix]
    from_node_id = node_data[node_id_ix]
    from_node = node_objects[from_node_id]

    for e in range(edge_count):
      edge_data = edges[edge_ix:(edge_ix + edge_field_count)]
      edge_type = edge_type_strings[edge_data[edge_type_ix]]

      # The interpretation of edge_name_or_index depends on the edge type.
      edge_name_or_index = edge_data[edge_name_or_index_ix]
      if edge_type == 'element' or edge_type == 'hidden':
        edge_name = str(edge_name_or_index)
      elif isinstance(edge_name_or_index, str):
        edge_name = edge_name_or_index
      else:
        edge_name = strings[edge_name_or_index]

      # "The index within the nodes array that this edge is connected to."
      to_node_ix = edge_data[edge_to_node_ix]
      to_node_data = nodes[to_node_ix:(to_node_ix + node_field_count)]
      to_node_id = to_node_data[node_id_ix]

      to_node = node_objects[to_node_id]

      edge = Edge(edge_type, from_node, edge_name, to_node)

      from_node.edges_from.append(edge)
      to_node.edges_to.append(edge)

      edge_ix += edge_field_count

  process_nodes(node_objects)


if __name__ == '__main__':
  main()
