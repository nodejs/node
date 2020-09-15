// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SelectionEvent extends CustomEvent {
  static name = "showentries";
  constructor(entries) {
    super(SelectionEvent.name, { bubbles: true, composed: true });
    if (!Array.isArray(entries) || entries.length == 0) {
      throw new Error("No valid entries selected!");
    }
    this.entries = entries;
  }
}

class FocusEvent extends CustomEvent {
  static name = "showentrydetail";
  constructor(entry) {
    super(FocusEvent.name, { bubbles: true, composed: true });
    this.entry = entry;
  }
}

class SelectTimeEvent extends CustomEvent {
  static name = 'timerangeselect';
  constructor(start, end) {
    super(SelectTimeEvent.name, { bubbles: true, composed: true });
    this.start = start;
    this.end = end;
  }
}

class SynchronizeSelectionEvent extends CustomEvent {
  static name = 'syncselection';
  constructor(start, end) {
    super(SynchronizeSelectionEvent.name, { bubbles: true, composed: true });
    this.start = start;
    this.end = end;
  }
}


export {
  SelectionEvent, FocusEvent, SelectTimeEvent,
  SynchronizeSelectionEvent
};
