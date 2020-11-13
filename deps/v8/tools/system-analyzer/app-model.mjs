// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class State {
  #timeSelection = { start: 0, end: Infinity };
  #map;
  #ic;
  #selectedMapLogEvents;
  #selectedIcLogEvents;
  #selectedSourcePositionLogEvents;
  #nofChunks;
  #chunks;
  #icTimeline;
  #mapTimeline;
  #minStartTime = Number.POSITIVE_INFINITY;
  #maxEndTime = Number.NEGATIVE_INFINITY;
  get minStartTime() {
    return this.#minStartTime;
  }
  get maxEndTime() {
    return this.#maxEndTime;
  }
  #updateTimeRange(timeline) {
    this.#minStartTime = Math.min(this.#minStartTime, timeline.startTime);
    this.#maxEndTime = Math.max(this.#maxEndTime, timeline.endTime);
  }
  get mapTimeline() {
    return this.#mapTimeline;
  }
  set mapTimeline(timeline) {
    this.#updateTimeRange(timeline);
    timeline.startTime = this.#minStartTime;
    timeline.endTime = this.#maxEndTime;
    this.#mapTimeline = timeline;
  }
  set icTimeline(timeline) {
    this.#updateTimeRange(timeline);
    timeline.startTime = this.#minStartTime;
    timeline.endTime = this.#maxEndTime;
    this.#icTimeline = timeline;
  }
  get icTimeline() {
    return this.#icTimeline;
  }
  set chunks(value) {
    //TODO(zcankara) split up between maps and ics, and every timeline track
    this.#chunks = value;
  }
  get chunks() {
    //TODO(zcankara) split up between maps and ics, and every timeline track
    return this.#chunks;
  }
  get nofChunks() {
    return this.#nofChunks;
  }
  set nofChunks(count) {
    this.#nofChunks = count;
  }
  get map() {
    //TODO(zcankara) rename as selectedMapEvents, array of selected events
    return this.#map;
  }
  set map(value) {
    //TODO(zcankara) rename as selectedMapEvents, array of selected events
    if (!value) return;
    this.#map = value;
  }
  get ic() {
    //TODO(zcankara) rename selectedICEvents, array of selected events
    return this.#ic;
  }
  set ic(value) {
    //TODO(zcankara) rename selectedIcEvents, array of selected events
    if (!value) return;
    this.#ic = value;
  }
  get selectedMapLogEvents() {
    return this.#selectedMapLogEvents;
  }
  set selectedMapLogEvents(value) {
    if (!value) return;
    this.#selectedMapLogEvents = value;
  }
  get selectedSourcePositionLogEvents() {
    return this.#selectedSourcePositionLogEvents;
  }
  set selectedSourcePositionLogEvents(value) {
    this.#selectedSourcePositionLogEvents = value;
  }
  get selectedIcLogEvents() {
    return this.#selectedIcLogEvents;
  }
  set selectedIcLogEvents(value) {
    if (!value) return;
    this.#selectedIcLogEvents = value;
  }
  get timeSelection() {
    return this.#timeSelection;
  }
  get entries() {
    if (!this.map) return {};
    return {
      map: this.map.id, time: this.map.time
    }
  }
}

export { State };
