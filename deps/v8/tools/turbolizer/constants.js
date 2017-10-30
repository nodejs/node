// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MAX_RANK_SENTINEL = 0;
var GRAPH_MARGIN = 250;
var WIDTH = 'width';
var HEIGHT = 'height';
var VISIBILITY = 'visibility';
var SOURCE_PANE_ID = 'left';
var SOURCE_COLLAPSE_ID = 'source-shrink';
var SOURCE_EXPAND_ID = 'source-expand';
var INTERMEDIATE_PANE_ID = 'middle';
var EMPTY_PANE_ID = 'empty';
var GRAPH_PANE_ID = 'graph';
var SCHEDULE_PANE_ID = 'schedule';
var GENERATED_PANE_ID = 'right';
var DISASSEMBLY_PANE_ID = 'disassembly';
var DISASSEMBLY_COLLAPSE_ID = 'disassembly-shrink';
var DISASSEMBLY_EXPAND_ID = 'disassembly-expand';
var COLLAPSE_PANE_BUTTON_VISIBLE = 'button-input';
var COLLAPSE_PANE_BUTTON_INVISIBLE = 'button-input-invisible';
var UNICODE_BLOCK = '&#9611;';
var PROF_COLS = [
  { perc:   0, col: { r: 255, g: 255, b: 255 } },
  { perc: 0.5, col: { r: 255, g: 255, b: 128 } },
  { perc:   5, col: { r: 255, g: 128, b:   0 } },
  { perc:  15, col: { r: 255, g:   0, b:   0 } },
  { perc: 100, col: { r:   0, g:   0, b:   0 } }
];
