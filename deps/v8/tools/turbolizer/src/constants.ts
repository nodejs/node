// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const MAX_RANK_SENTINEL = 0;
const GRAPH_MARGIN = 250;
const WIDTH = 'width';
const HEIGHT = 'height';
const VISIBILITY = 'visibility';
const SOURCE_PANE_ID = 'left';
const SOURCE_COLLAPSE_ID = 'source-shrink';
const SOURCE_EXPAND_ID = 'source-expand';
const INTERMEDIATE_PANE_ID = 'middle';
const GRAPH_PANE_ID = 'graph';
const SCHEDULE_PANE_ID = 'schedule';
const GENERATED_PANE_ID = 'right';
const DISASSEMBLY_PANE_ID = 'disassembly';
const DISASSEMBLY_COLLAPSE_ID = 'disassembly-shrink';
const DISASSEMBLY_EXPAND_ID = 'disassembly-expand';
const COLLAPSE_PANE_BUTTON_VISIBLE = 'button-input';
const COLLAPSE_PANE_BUTTON_INVISIBLE = 'button-input-invisible';
const UNICODE_BLOCK = '&#9611;';
const PROF_COLS = [
  { perc: 0, col: { r: 255, g: 255, b: 255 } },
  { perc: 0.5, col: { r: 255, g: 255, b: 128 } },
  { perc: 5, col: { r: 255, g: 128, b: 0 } },
  { perc: 15, col: { r: 255, g: 0, b: 0 } },
  { perc: 100, col: { r: 0, g: 0, b: 0 } }
];
