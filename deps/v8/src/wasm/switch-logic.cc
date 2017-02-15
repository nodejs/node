// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/switch-logic.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
CaseNode* CreateBst(ZoneVector<CaseNode*>* nodes, size_t begin, size_t end) {
  if (end < begin) {
    return nullptr;
  } else if (end == begin) {
    return nodes->at(begin);
  } else {
    size_t root_index = (begin + end) / 2;
    CaseNode* root = nodes->at(root_index);
    if (root_index != 0) {
      root->left = CreateBst(nodes, begin, root_index - 1);
    }
    root->right = CreateBst(nodes, root_index + 1, end);
    return root;
  }
}
}  // namespace

CaseNode* OrderCases(ZoneVector<int>* cases, Zone* zone) {
  const int max_distance = 2;
  const int min_size = 4;
  if (cases->empty()) {
    return nullptr;
  }
  std::sort(cases->begin(), cases->end());
  ZoneVector<size_t> table_breaks(zone);
  for (size_t i = 1; i < cases->size(); ++i) {
    if (cases->at(i) - cases->at(i - 1) > max_distance) {
      table_breaks.push_back(i);
    }
  }
  table_breaks.push_back(cases->size());
  ZoneVector<CaseNode*> nodes(zone);
  size_t curr_pos = 0;
  for (size_t i = 0; i < table_breaks.size(); ++i) {
    size_t break_pos = table_breaks[i];
    if (break_pos - curr_pos >= min_size) {
      int begin = cases->at(curr_pos);
      int end = cases->at(break_pos - 1);
      nodes.push_back(new (zone) CaseNode(begin, end));
      curr_pos = break_pos;
    } else {
      for (; curr_pos < break_pos; curr_pos++) {
        nodes.push_back(new (zone)
                            CaseNode(cases->at(curr_pos), cases->at(curr_pos)));
      }
    }
  }
  return CreateBst(&nodes, 0, nodes.size() - 1);
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
