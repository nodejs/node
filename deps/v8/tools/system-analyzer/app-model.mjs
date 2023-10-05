// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class State {
  _timeSelection = {start: 0, end: Infinity};
  _map;
  _ic;
  _selectedMapLogEntries;
  _selectedIcLogEntries;
  _selectedDeoptLogEntries;
  _selecteCodeLogEntries;
  _selectedSourcePositions;
  _nofChunks;
  _chunks;
  _icTimeline;
  _mapTimeline;
  _deoptTimeline;
  _codeTimeline;
  _tickTimeline;
  _timerTimeline;
  _minStartTime = Number.POSITIVE_INFINITY;
  _maxEndTime = Number.NEGATIVE_INFINITY;

  get minStartTime() {
    return this._minStartTime;
  }

  get maxEndTime() {
    return this._maxEndTime;
  }

  selectTimeRange(start, end) {
    this.timeSelection.start = start;
    this.timeSelection.end = end;
    if (start == 0 && end == Infinity) {
      this.timelines.forEach(each => each.clearSelection());
    } else {
      this.timelines.forEach(each => each.selectTimeRange(start, end));
    }
  }

  setTimelines(
      mapTimeline, icTimeline, deoptTimeline, codeTimeline, tickTimeline,
      timerTimeline) {
    this._mapTimeline = mapTimeline;
    this._icTimeline = icTimeline;
    this._deoptTimeline = deoptTimeline;
    this._codeTimeline = codeTimeline;
    this._tickTimeline = tickTimeline;
    this._timerTimeline = timerTimeline;
    for (let timeline of arguments) {
      if (timeline === undefined) return;
      this._minStartTime = Math.min(this._minStartTime, timeline.startTime);
      this._maxEndTime = Math.max(this._maxEndTime, timeline.endTime);
    }
    for (let timeline of arguments) {
      timeline.startTime = this._minStartTime;
      timeline.endTime = this._maxEndTime;
    }
  }

  get mapTimeline() {
    return this._mapTimeline;
  }

  get icTimeline() {
    return this._icTimeline;
  }

  get deoptTimeline() {
    return this._deoptTimeline;
  }

  get codeTimeline() {
    return this._codeTimeline;
  }

  get tickTimeline() {
    return this._tickTimeline;
  }

  get timerTimeline() {
    return this._timerTimeline;
  }

  get timelines() {
    return [
      this._mapTimeline, this._icTimeline, this._deoptTimeline,
      this._codeTimeline, this._tickTimeline, this._timerTimeline
    ];
  }

  set chunks(value) {
    // TODO(zcankara) split up between maps and ics, and every timeline track
    this._chunks = value;
  }

  get chunks() {
    // TODO(zcankara) split up between maps and ics, and every timeline track
    return this._chunks;
  }

  get nofChunks() {
    return this._nofChunks;
  }
  set nofChunks(count) {
    this._nofChunks = count;
  }
  get map() {
    // TODO(zcankara) rename as selectedMapEvents, array of selected events
    return this._map;
  }
  set map(value) {
    // TODO(zcankara) rename as selectedMapEvents, array of selected events
    if (!value) return;
    this._map = value;
  }
  get ic() {
    // TODO(zcankara) rename selectedICEvents, array of selected events
    return this._ic;
  }
  set ic(value) {
    // TODO(zcankara) rename selectedIcEvents, array of selected events
    if (!value) return;
    this._ic = value;
  }
  get selectedMapLogEntries() {
    return this._selectedMapLogEntries;
  }
  set selectedMapLogEntries(value) {
    if (!value) return;
    this._selectedMapLogEntries = value;
  }
  get selectedSourcePositions() {
    return this._selectedSourcePositions;
  }
  set selectedSourcePositions(value) {
    this._selectedSourcePositions = value;
  }
  get selectedIcLogEntries() {
    return this._selectedIcLogEntries;
  }
  set selectedIcLogEntries(value) {
    if (!value) return;
    this._selectedIcLogEntries = value;
  }
  get timeSelection() {
    return this._timeSelection;
  }
  get entries() {
    if (!this.map) return {};
    return {
    map:
      this.map.id, time: this.map.time
    }
  }
}

export {State};
