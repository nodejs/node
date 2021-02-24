// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class AppEvent extends CustomEvent {
  constructor(name) {
    super(name, {bubbles: true, composed: true});
  }
}

export class SelectionEvent extends AppEvent {
  // TODO: turn into static class fields once Safari supports it.
  static get name() {
    return 'select';
  }

  constructor(entries) {
    super(SelectionEvent.name);
    if (!Array.isArray(entries) || entries.length == 0) {
      throw new Error('No valid entries selected!');
    }
    this.entries = entries;
  }
}

export class SelectRelatedEvent extends AppEvent {
  static get name() {
    return 'selectrelated';
  }

  constructor(entry) {
    super(SelectRelatedEvent.name);
    this.entry = entry;
  }
}

export class FocusEvent extends AppEvent {
  static get name() {
    return 'showentrydetail';
  }

  constructor(entry) {
    super(FocusEvent.name);
    this.entry = entry;
  }
}

export class SelectTimeEvent extends AppEvent {
  static get name() {
    return 'timerangeselect';
  }

  constructor(start = 0, end = Infinity) {
    super(SelectTimeEvent.name);
    this.start = start;
    this.end = end;
  }
}

export class SynchronizeSelectionEvent extends AppEvent {
  static get name() {
    return 'syncselection';
  }

  constructor(start, end) {
    super(SynchronizeSelectionEvent.name);
    this.start = start;
    this.end = end;
  }
}

export class ToolTipEvent extends AppEvent {
  static get name() {
    return 'showtooltip';
  }

  constructor(content, positionOrTargetNode) {
    super(ToolTipEvent.name);
    this._content = content;
    if (!positionOrTargetNode && !node) {
      throw Error('Either provide a valid position or targetNode');
    }
    this._positionOrTargetNode = positionOrTargetNode;
  }

  get content() {
    return this._content;
  }

  get positionOrTargetNode() {
    return this._positionOrTargetNode;
  }
}
