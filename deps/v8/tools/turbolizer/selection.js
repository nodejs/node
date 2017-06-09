// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Selection = function(handler) {
  this.handler = handler;
  this.selectionBase = null;
  this.lastSelection = null;
  this.selection = new Set();
}


Selection.prototype.isEmpty = function() {
  return this.selection.size == 0;
}


Selection.prototype.clear = function() {
  var handler = this.handler;
  this.selectionBase = null;
  this.lastSelection = null;
  handler.select(this.selection, false);
  handler.clear();
  this.selection = new Set();
}


count = 0;

Selection.prototype.select = function(s, isSelected) {
  var handler = this.handler;
  if (!(Symbol.iterator in Object(s))) { s = [s]; }
  if (isSelected) {
    let first = true;
    for (let i of s) {
      if (first) {
        this.selectionBase = i;
        this.lastSelection = i;
        first = false;
      }
      this.selection.add(i);
    }
    handler.select(this.selection, true);
  } else {
    let unselectSet = new Set();
    for (let i of s) {
      if (this.selection.has(i)) {
        unselectSet.add(i);
        this.selection.delete(i);
      }
    }
    handler.select(unselectSet, false);
  }
}


Selection.prototype.extendTo = function(pos) {
  if (pos == this.lastSelection || this.lastSelection === null) return;

  var handler = this.handler;
  var pos_diff = handler.selectionDifference(pos, true, this.lastSelection, false);
  var unselect_diff = [];
  if (pos_diff.length == 0) {
    pos_diff = handler.selectionDifference(this.selectionBase, false, pos, true);
    if (pos_diff.length != 0) {
      unselect_diff = handler.selectionDifference(this.lastSelection, true, this.selectionBase, false);
      this.selection = new Set();
      this.selection.add(this.selectionBase);
      for (var d of pos_diff) {
        this.selection.add(d);
      }
    } else {
      unselect_diff = handler.selectionDifference(this.lastSelection, true, pos, false);
      for (var d of unselect_diff) {
        this.selection.delete(d);
      }
    }
  } else {
    unselect_diff = handler.selectionDifference(this.selectionBase, false, this.lastSelection, true);
    if (unselect_diff != 0) {
      pos_diff = handler.selectionDifference(pos, true, this.selectionBase, false);
      if (pos_diff.length == 0) {
        unselect_diff = handler.selectionDifference(pos, false, this.lastSelection, true);
      }
      for (var d of unselect_diff) {
        this.selection.delete(d);
      }
    }
    if (pos_diff.length != 0) {
      for (var d of pos_diff) {
        this.selection.add(d);
      }
    }
  }
  handler.select(unselect_diff, false);
  handler.select(pos_diff, true);
  this.lastSelection = pos;
}


Selection.prototype.detachSelection = function() {
  var result = new Set();
  for (var i of this.selection) {
    result.add(i);
  }
  this.clear();
  return result;
}
