// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SourcePosition} from '../profile.mjs';

import {State} from './app-model.mjs';
import {FocusEvent, SelectionEvent, SelectTimeEvent} from './events.mjs';
import {$} from './helper.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {MapLogEntry} from './log/map.mjs';
import {Processor} from './processor.mjs';

class App {
  _state;
  _view;
  _navigation;
  _startupPromise;
  constructor(
      fileReaderId, mapPanelId, mapStatsPanelId, timelinePanelId, icPanelId,
      mapTrackId, icTrackId, deoptTrackId, sourcePanelId) {
    this._view = {
      __proto__: null,
      logFileReader: $(fileReaderId),
      icPanel: $(icPanelId),
      mapPanel: $(mapPanelId),
      mapStatsPanel: $(mapStatsPanelId),
      timelinePanel: $(timelinePanelId),
      mapTrack: $(mapTrackId),
      icTrack: $(icTrackId),
      deoptTrack: $(deoptTrackId),
      sourcePanel: $(sourcePanelId)
    };
    this.toggleSwitch = $('.theme-switch input[type="checkbox"]');
    this.toggleSwitch.addEventListener('change', (e) => this.switchTheme(e));
    this._view.logFileReader.addEventListener(
        'fileuploadstart', (e) => this.handleFileUploadStart(e));
    this._view.logFileReader.addEventListener(
        'fileuploadend', (e) => this.handleFileUploadEnd(e));
    this._startupPromise = this.runAsyncInitialize();
  }

  async runAsyncInitialize() {
    await Promise.all([
      import('./ic-panel.mjs'),
      import('./timeline-panel.mjs'),
      import('./stats-panel.mjs'),
      import('./map-panel.mjs'),
      import('./source-panel.mjs'),
    ]);
    document.addEventListener(
        'keydown', e => this._navigation?.handleKeyDown(e));
    document.addEventListener(
        SelectionEvent.name, e => this.handleShowEntries(e));
    document.addEventListener(
        FocusEvent.name, e => this.handleShowEntryDetail(e));
    document.addEventListener(
        SelectTimeEvent.name, e => this.handleTimeRangeSelect(e));
  }

  handleShowEntries(e) {
    if (e.entries[0] instanceof MapLogEntry) {
      this.showMapEntries(e.entries);
    } else if (e.entries[0] instanceof IcLogEntry) {
      this.showIcEntries(e.entries);
    } else if (e.entries[0] instanceof SourcePosition) {
      this.showSourcePositionEntries(e.entries);
    } else {
      throw new Error('Unknown selection type!');
    }
    e.stopPropagation();
  }
  showMapEntries(entries) {
    this._state.selectedMapLogEntries = entries;
    this._view.mapPanel.selectedMapLogEntries = entries;
    this._view.mapStatsPanel.selectedLogEntries = entries;
  }
  showIcEntries(entries) {
    this._state.selectedIcLogEntries = entries;
    this._view.icPanel.selectedLogEntries = entries;
  }
  showDeoptEntries(entries) {
    this._state.selectedDeoptLogEntries = entries;
  }
  showSourcePositionEntries(entries) {
    // TODO: Handle multiple source position selection events
    this._view.sourcePanel.selectedSourcePositions = entries
  }

  handleTimeRangeSelect(e) {
    this.selectTimeRange(e.start, e.end);
    e.stopPropagation();
  }

  selectTimeRange(start, end) {
    this._state.selectTimeRange(start, end);
    this.showMapEntries(this._state.mapTimeline.selection);
    this.showIcEntries(this._state.icTimeline.selection);
    this.showDeoptEntries(this._state.deoptTimeline.selection);
    this._view.timelinePanel.timeSelection = {start, end};
  }

  handleShowEntryDetail(e) {
    if (e.entry instanceof MapLogEntry) {
      this.selectMapLogEntry(e.entry);
    } else if (e.entry instanceof IcLogEntry) {
      this.selectICLogEntry(e.entry);
    } else if (e.entry instanceof SourcePosition) {
      this.selectSourcePosition(e.entry);
    } else {
      throw new Error('Unknown selection type!');
    }
    e.stopPropagation();
  }
  selectMapLogEntry(entry) {
    this._state.map = entry;
    this._view.mapTrack.selectedEntry = entry;
    this._view.mapPanel.map = entry;
  }
  selectICLogEntry(entry) {
    this._state.ic = entry;
    this._view.icPanel.selectedLogEntries = [entry];
  }
  selectSourcePosition(sourcePositions) {
    if (!sourcePositions.script) return;
    this._view.sourcePanel.selectedSourcePositions = [sourcePositions];
  }

  handleFileUploadStart(e) {
    this.restartApp();
    $('#container').className = 'initial';
  }

  restartApp() {
    this._state = new State();
    this._navigation = new Navigation(this._state, this._view);
  }

  async handleFileUploadEnd(e) {
    await this._startupPromise;
    try {
      const processor = new Processor(e.detail);
      const mapTimeline = processor.mapTimeline;
      const icTimeline = processor.icTimeline;
      const deoptTimeline = processor.deoptTimeline;
      this._state.mapTimeline = mapTimeline;
      this._state.icTimeline = icTimeline;
      this._state.deoptTimeline = deoptTimeline;
      // Transitions must be set before timeline for stats panel.
      this._view.mapPanel.timeline = mapTimeline;
      this._view.mapTrack.data = mapTimeline;
      this._view.mapStatsPanel.transitions =
          this._state.mapTimeline.transitions;
      this._view.mapStatsPanel.timeline = mapTimeline;
      this._view.icPanel.timeline = icTimeline;
      this._view.icTrack.data = icTimeline;
      this._view.deoptTrack.data = deoptTimeline;
      this._view.sourcePanel.data = processor.scripts
    } catch (e) {
      this._view.logFileReader.error = 'Log file contains errors!'
      throw (e);
    } finally {
      $('#container').className = 'loaded';
      this.fileLoaded = true;
    }
  }

  refreshTimelineTrackView() {
    this._view.mapTrack.data = this._state.mapTimeline;
    this._view.icTrack.data = this._state.icTimeline;
    this._view.deoptTrack.data = this._state.deoptTimeline;
  }

  switchTheme(event) {
    document.documentElement.dataset.theme =
        event.target.checked ? 'light' : 'dark';
    if (this.fileLoaded) {
      this.refreshTimelineTrackView();
    }
  }
}

class Navigation {
  _view;
  constructor(state, view) {
    this.state = state;
    this._view = view;
  }
  get map() {
    return this.state.map
  }
  set map(value) {
    this.state.map = value
  }
  get chunks() {
    return this.state.mapTimeline.chunks;
  }
  increaseTimelineResolution() {
    this._view.timelinePanel.nofChunks *= 1.5;
    this.state.nofChunks *= 1.5;
  }
  decreaseTimelineResolution() {
    this._view.timelinePanel.nofChunks /= 1.5;
    this.state.nofChunks /= 1.5;
  }
  selectNextEdge() {
    if (!this.map) return;
    if (this.map.children.length != 1) return;
    this.map = this.map.children[0].to;
    this._view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this._view.mapPanel.map = this.map;
  }
  selectPrevEdge() {
    if (!this.map) return;
    if (!this.map.parent()) return;
    this.map = this.map.parent();
    this._view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this._view.mapPanel.map = this.map;
  }
  selectDefaultMap() {
    this.map = this.chunks[0].at(0);
    this._view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this._view.mapPanel.map = this.map;
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
    this._view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this._view.mapPanel.map = this.map;
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
    this._view.mapTrack.selectedEntry = this.map;
    this.updateUrl();
    this._view.mapPanel.map = this.map;
  }
  updateUrl() {
    let entries = this.state.entries;
    let params = new URLSearchParams(entries);
    window.history.pushState(entries, '', '?' + params.toString());
  }
  handleKeyDown(event) {
    switch (event.key) {
      case 'ArrowUp':
        event.preventDefault();
        if (event.shiftKey) {
          this.selectPrevEdge();
        } else {
          this.moveInChunk(-1);
        }
        return false;
      case 'ArrowDown':
        event.preventDefault();
        if (event.shiftKey) {
          this.selectNextEdge();
        } else {
          this.moveInChunk(1);
        }
        return false;
      case 'ArrowLeft':
        this.moveInChunks(false);
        break;
      case 'ArrowRight':
        this.moveInChunks(true);
        break;
      case '+':
        this.increaseTimelineResolution();
        break;
      case '-':
        this.decreaseTimelineResolution();
        break;
    }
  }
}

export {App};
