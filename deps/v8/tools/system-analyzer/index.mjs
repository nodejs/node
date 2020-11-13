// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import { SelectionEvent, FocusEvent, SelectTimeEvent } from "./events.mjs";
import { State } from "./app-model.mjs";
import { MapLogEvent } from "./log/map.mjs";
import { IcLogEvent } from "./log/ic.mjs";
import Processor from "./processor.mjs";
import { SourcePosition } from  "../profile.mjs";
import { $ } from "./helper.mjs";
import "./ic-panel.mjs";
import "./timeline-panel.mjs";
import "./map-panel.mjs";
import "./log-file-reader.mjs";
import "./source-panel.mjs";


class App {
  #state;
  #view;
  #navigation;
  constructor(fileReaderId, mapPanelId, timelinePanelId,
    icPanelId, mapTrackId, icTrackId, sourcePanelId) {
    this.#view = {
      logFileReader: $(fileReaderId),
      icPanel: $(icPanelId),
      mapPanel: $(mapPanelId),
      timelinePanel: $(timelinePanelId),
      mapTrack: $(mapTrackId),
      icTrack: $(icTrackId),
      sourcePanel: $(sourcePanelId)
    };
    this.#state = new State();
    this.#navigation = new Navigation(this.#state, this.#view);
    document.addEventListener('keydown',
      e => this.#navigation.handleKeyDown(e));
    this.toggleSwitch = $('.theme-switch input[type="checkbox"]');
    this.toggleSwitch.addEventListener("change", (e) => this.switchTheme(e));
    this.#view.logFileReader.addEventListener("fileuploadstart", (e) =>
      this.handleFileUpload(e)
    );
    this.#view.logFileReader.addEventListener("fileuploadend", (e) =>
      this.handleDataUpload(e)
    );
    Object.entries(this.#view).forEach(([_, panel]) => {
      panel.addEventListener(SelectionEvent.name,
        e => this.handleShowEntries(e));
      panel.addEventListener(FocusEvent.name,
        e => this.handleShowEntryDetail(e));
      panel.addEventListener(SelectTimeEvent.name,
        e => this.handleTimeRangeSelect(e));
    });
  }
  handleShowEntries(e) {
    if (e.entries[0] instanceof MapLogEvent) {
      this.showMapEntries(e.entries);
    } else if (e.entries[0] instanceof IcLogEvent) {
      this.showIcEntries(e.entries);
    } else if (e.entries[0] instanceof SourcePosition) {
      this.showSourcePositionEntries(e.entries);
    } else {
      throw new Error("Unknown selection type!");
    }
  }
  showMapEntries(entries) {
    this.#state.selectedMapLogEvents = entries;
    this.#view.mapPanel.selectedMapLogEvents = this.#state.selectedMapLogEvents;
  }
  showIcEntries(entries) {
    this.#state.selectedIcLogEvents = entries;
    this.#view.icPanel.selectedLogEvents = this.#state.selectedIcLogEvents;
  }
  showSourcePositionEntries(entries) {
    //TODO(zcankara) Handle multiple source position selection events
    this.#view.sourcePanel.selectedSourcePositions = entries;
  }

  handleTimeRangeSelect(e) {
    this.selectTimeRange(e.start, e.end);
  }
  handleShowEntryDetail(e) {
    if (e.entry instanceof MapLogEvent) {
      this.selectMapLogEvent(e.entry);
    } else if (e.entry instanceof IcLogEvent) {
      this.selectICLogEvent(e.entry);
    } else if (e.entry instanceof SourcePosition) {
      this.selectSourcePositionEvent(e.entry);
    } else {
      throw new Error("Unknown selection type!");
    }
  }
  selectTimeRange(start, end) {
    this.#state.timeSelection.start = start;
    this.#state.timeSelection.end = end;
    this.#state.icTimeline.selectTimeRange(start, end);
    this.#state.mapTimeline.selectTimeRange(start, end);
    this.#view.mapPanel.selectedMapLogEvents =
      this.#state.mapTimeline.selection;
    this.#view.icPanel.selectedLogEvents = this.#state.icTimeline.selection;
  }
  selectMapLogEvent(entry) {
    this.#state.map = entry;
    this.#view.mapTrack.selectedEntry = entry;
    this.#view.mapPanel.map = entry;
  }
  selectICLogEvent(entry) {
    this.#state.ic = entry;
    this.#view.icPanel.selectedLogEvents = [entry];
  }
  selectSourcePositionEvent(sourcePositions) {
    if (!sourcePositions.script) return;
    this.#view.sourcePanel.selectedSourcePositions = [sourcePositions];
  }

  handleFileUpload(e) {
    this.restartApp();
    $("#container").className = "initial";
  }
  restartApp() {
    this.#state = new State();
    this.#navigation = new Navigation(this.#state, this.#view);
  }
  // Event log processing
  handleLoadTextProcessor(text) {
    let logProcessor = new Processor();
    logProcessor.processString(text);
    return logProcessor;
  }

  // call when a new file uploaded
  handleDataUpload(e) {
    if (!e.detail) return;
    $("#container").className = "loaded";
    // instantiate the app logic
    let fileData = e.detail;
    const processor = this.handleLoadTextProcessor(fileData.chunk);
    const mapTimeline = processor.mapTimeline;
    const icTimeline = processor.icTimeline;
    //TODO(zcankara) Make sure only one instance of src event map ic id match
    // Load map log events timeline.
    this.#state.mapTimeline = mapTimeline;
    // Transitions must be set before timeline for stats panel.
    this.#view.mapPanel.transitions = this.#state.mapTimeline.transitions;
    this.#view.mapTrack.data = mapTimeline;
    this.#state.chunks = this.#view.mapTrack.chunks;
    this.#view.mapPanel.timeline = mapTimeline;
    // Load ic log events timeline.
    this.#state.icTimeline = icTimeline;
    this.#view.icPanel.timeline = icTimeline;
    this.#view.icTrack.data = icTimeline;
    this.#view.sourcePanel.data = processor.scripts
    this.fileLoaded = true;
  }

  refreshTimelineTrackView() {
    this.#view.mapTrack.data = this.#state.mapTimeline;
    this.#view.icTrack.data = this.#state.icTimeline;
  }

  switchTheme(event) {
    document.documentElement.dataset.theme = event.target.checked
      ? "light"
      : "dark";
    if (this.fileLoaded) {
      this.refreshTimelineTrackView();
    }
  }
}

class Navigation {
  #view;
  constructor(state, view) {
    this.state = state;
    this.#view = view;
  }
  get map() {
    return this.state.map
  }
  set map(value) {
    this.state.map = value
  }
  get chunks() {
    return this.state.chunks
  }
  increaseTimelineResolution() {
    this.#view.timelinePanel.nofChunks *= 1.5;
    this.state.nofChunks *= 1.5;
  }
  decreaseTimelineResolution() {
    this.#view.timelinePanel.nofChunks /= 1.5;
    this.state.nofChunks /= 1.5;
  }
  selectNextEdge() {
    if (!this.map) return;
    if (this.map.children.length != 1) return;
    this.map = this.map.children[0].to;
    this.#view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this.#view.mapPanel.map = this.map;
  }
  selectPrevEdge() {
    if (!this.map) return;
    if (!this.map.parent()) return;
    this.map = this.map.parent();
    this.#view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this.#view.mapPanel.map = this.map;
  }
  selectDefaultMap() {
    this.map = this.chunks[0].at(0);
    this.#view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this.#view.mapPanel.map = this.map;
  }
  moveInChunks(next) {
    if (!this.map) return this.selectDefaultMap();
    let chunkIndex = this.map.chunkIndex(this.chunks);
    let chunk = this.chunks[chunkIndex];
    let index = chunk.indexOf(this.map);
    if (next) {
      chunk = chunk.next(this.chunks);
    } else {
      chunk = chunk.prev(this.chunks);
    }
    if (!chunk) return;
    index = Math.min(index, chunk.size() - 1);
    this.map = chunk.at(index);
    this.#view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this.#view.mapPanel.map = this.map;
  }
  moveInChunk(delta) {
    if (!this.map) return this.selectDefaultMap();
    let chunkIndex = this.map.chunkIndex(this.chunks)
    let chunk = this.chunks[chunkIndex];
    let index = chunk.indexOf(this.map) + delta;
    let map;
    if (index < 0) {
      map = chunk.prev(this.chunks).last();
    } else if (index >= chunk.size()) {
      map = chunk.next(this.chunks).first()
    } else {
      map = chunk.at(index);
    }
    this.map = map;
    this.#view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this.#view.mapPanel.map = this.map;
  }
  updateUrl() {
    let entries = this.state.entries;
    let params = new URLSearchParams(entries);
    window.history.pushState(entries, '', '?' + params.toString());
  }
  handleKeyDown(event) {
    switch (event.key) {
      case "ArrowUp":
        event.preventDefault();
        if (event.shiftKey) {
          this.selectPrevEdge();
        } else {
          this.moveInChunk(-1);
        }
        return false;
      case "ArrowDown":
        event.preventDefault();
        if (event.shiftKey) {
          this.selectNextEdge();
        } else {
          this.moveInChunk(1);
        }
        return false;
      case "ArrowLeft":
        this.moveInChunks(false);
        break;
      case "ArrowRight":
        this.moveInChunks(true);
        break;
      case "+":
        this.increaseTimelineResolution();
        break;
      case "-":
        this.decreaseTimelineResolution();
        break;
    }
  }
}

export { App };
