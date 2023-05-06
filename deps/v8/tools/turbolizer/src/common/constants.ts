// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const TRACE_LAYOUT = false;
export const TRACE_HISTORY = false;

export const MAX_RANK_SENTINEL = 0;
export const BEZIER_CONSTANT = 0.3;
export const GRAPH_MARGIN = 250;
export const TURBOSHAFT_NODE_X_INDENT = 25;
export const TURBOSHAFT_COLLAPSE_ICON_X_INDENT = 20;
export const TURBOSHAFT_BLOCK_BORDER_RADIUS = 35;
export const TURBOSHAFT_NODE_BORDER_RADIUS = 20;
export const TURBOSHAFT_BLOCK_ROW_SEPARATION = 200;
export const ARROW_HEAD_HEIGHT = 7;
export const DEFAULT_NODE_BUBBLE_RADIUS = 12;
export const NODE_INPUT_WIDTH = 50;
export const MINIMUM_NODE_OUTPUT_APPROACH = 15;
export const MINIMUM_EDGE_SEPARATION = 20;
export const MINIMUM_NODE_INPUT_APPROACH = 15 + 2 * DEFAULT_NODE_BUBBLE_RADIUS;
export const DEFAULT_NODE_ROW_SEPARATION = 150;
export const SOURCE_PANE_DEFAULT_PERCENT = 1 / 4;
export const DISASSEMBLY_PANE_DEFAULT_PERCENT = 3 / 4;
export const RANGES_PANE_HEIGHT_DEFAULT_PERCENT = 3 / 4;
export const RANGES_PANE_WIDTH_DEFAULT_PERCENT = 1 / 2;
export const HISTORY_DEFAULT_HEIGHT_PERCENT = 1 / 3.5;
export const HISTORY_CONTENT_INDENT = 8;
export const HISTORY_SCROLLBAR_WIDTH = 6;
export const CLOSE_BUTTON_RADIUS = 25;
export const RESIZER_RANGES_HEIGHT_BUFFER_PERCENTAGE = 5;
export const ROW_GROUP_SIZE = 20;
export const POSITIONS_PER_INSTRUCTION = 4;
export const FIXED_REGISTER_LABEL_WIDTH = 6;
export const FLIPPED_REGISTER_WIDTH_BUFFER = 5;
// Required due to the css grid-template-rows and grid-template-columns being limited
// to 1000 places. Regardless of this, a limit is required at some point due
// to performance issues.
export const MAX_NUM_POSITIONS = 999;
export const SESSION_STORAGE_PREFIX = "ranges-setting-";
export const INTERVAL_TEXT_FOR_NONE = "none";
export const INTERVAL_TEXT_FOR_CONST = "const";
export const INTERVAL_TEXT_FOR_STACK = "stack:";
export const VIRTUAL_REGISTER_ID_PREFIX = "virt_";
export const HISTORY_ID = "history";
export const MULTIVIEW_ID = "multiview";
export const RESIZER_RANGES_ID = "resizer-ranges";
export const SHOW_HIDE_RANGES_ID = "show-hide-ranges";
export const SHOW_HIDE_SOURCE_ID = "show-hide-source";
export const SHOW_HIDE_DISASSEMBLY_ID = "show-hide-disassembly";
export const SOURCE_PANE_ID = "left";
export const SOURCE_COLLAPSE_ID = "source-shrink";
export const SOURCE_EXPAND_ID = "source-expand";
export const INTERMEDIATE_PANE_ID = "middle";
export const GRAPH_PANE_ID = "graph";
export const SCHEDULE_PANE_ID = "schedule";
export const SEQUENCE_PANE_ID = "sequence";
export const GENERATED_PANE_ID = "right";
export const DISASSEMBLY_PANE_ID = "disassembly";
export const DISASSEMBLY_COLLAPSE_ID = "disassembly-shrink";
export const DISASSEMBLY_EXPAND_ID = "disassembly-expand";
export const RANGES_PANE_ID = "ranges";
export const RANGES_COLLAPSE_VERT_ID = "ranges-shrink-vert";
export const RANGES_EXPAND_VERT_ID = "ranges-expand-vert";
export const RANGES_COLLAPSE_HOR_ID = "ranges-shrink-hor";
export const RANGES_EXPAND_HOR_ID = "ranges-expand-hor";
export const UNICODE_BLOCK = "&#9611;";
export const PROF_COLS = [
  { perc: 0, col: { r: 255, g: 255, b: 255 } },
  { perc: 0.5, col: { r: 255, g: 255, b: 128 } },
  { perc: 5, col: { r: 255, g: 128, b: 0 } },
  { perc: 15, col: { r: 255, g: 0, b: 0 } },
  { perc: 100, col: { r: 0, g: 0, b: 0 } }
];
