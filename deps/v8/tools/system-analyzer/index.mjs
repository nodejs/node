// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Script, SourcePosition} from '../profile.mjs';

import {State} from './app-model.mjs';
import {ApiLogEntry} from './log/api.mjs';
import {DeoptLogEntry} from './log/code.mjs';
import {CodeLogEntry} from './log/code.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {MapLogEntry} from './log/map.mjs';
import {Processor} from './processor.mjs';
import {FocusEvent, SelectionEvent, SelectRelatedEvent, SelectTimeEvent, ToolTipEvent,} from './view/events.mjs';
import {$, CSSColor, groupBy} from './view/helper.mjs';

class App {
  _state;
  _view;
  _navigation;
  _startupPromise;
  constructor() {
    this._view = {
      __proto__: null,
      logFileReader: $('#log-file-reader'),

      timelinePanel: $('#timeline-panel'),
      mapTrack: $('#map-track'),
      icTrack: $('#ic-track'),
      deoptTrack: $('#deopt-track'),
      codeTrack: $('#code-track'),
      apiTrack: $('#api-track'),

      icList: $('#ic-list'),
      mapList: $('#map-list'),
      codeList: $('#code-list'),
      deoptList: $('#deopt-list'),
      apiList: $('#api-list'),

      mapPanel: $('#map-panel'),
      codePanel: $('#code-panel'),
      scriptPanel: $('#script-panel'),

      toolTip: $('#tool-tip'),
    };
    this.toggleSwitch = $('.theme-switch input[type="checkbox"]');
    this.toggleSwitch.addEventListener('change', (e) => this.switchTheme(e));
    this._view.logFileReader.addEventListener(
        'fileuploadstart', (e) => this.handleFileUploadStart(e));
    this._view.logFileReader.addEventListener(
        'fileuploadend', (e) => this.handleFileUploadEnd(e));
    this._startupPromise = this.runAsyncInitialize();
  }

  static get allEventTypes() {
    return new Set([
      SourcePosition, MapLogEntry, IcLogEntry, ApiLogEntry, CodeLogEntry,
      DeoptLogEntry
    ]);
  }

  async runAsyncInitialize() {
    await Promise.all([
      import('./view/list-panel.mjs'),
      import('./view/timeline-panel.mjs'),
      import('./view/map-panel.mjs'),
      import('./view/script-panel.mjs'),
      import('./view/code-panel.mjs'),
      import('./view/tool-tip.mjs'),
    ]);
    document.addEventListener(
        'keydown', e => this._navigation?.handleKeyDown(e));
    document.addEventListener(
        SelectRelatedEvent.name, e => this.handleSelectRelatedEntries(e));
    document.addEventListener(
        SelectionEvent.name, e => this.handleSelectEntries(e))
    document.addEventListener(
        FocusEvent.name, e => this.handleFocusLogEntryl(e));
    document.addEventListener(
        SelectTimeEvent.name, e => this.handleTimeRangeSelect(e));
    document.addEventListener(ToolTipEvent.name, e => this.handleToolTip(e));
  }

  handleSelectRelatedEntries(e) {
    e.stopImmediatePropagation();
    this.selectRelatedEntries(e.entry);
  }

  selectRelatedEntries(entry) {
    let entries = [entry];
    switch (entry.constructor) {
      case SourcePosition:
        entries = entries.concat(entry.entries);
        break;
      case MapLogEntry:
        entries = this._state.icTimeline.filter(each => each.map === entry);
        break;
      case IcLogEntry:
        if (entry.map) entries.push(entry.map);
        break;
      case ApiLogEntry:
        break;
      case CodeLogEntry:
        break;
      case DeoptLogEntry:
        // TODO select map + code entries
        if (entry.fileSourcePosition) entries.push(entry.fileSourcePosition);
        break;
      case Script:
        entries = entry.entries.concat(entry.sourcePositions);
        break;
      default:
        throw new Error('Unknown selection type!');
    }
    if (entry.sourcePosition) {
      entries.push(entry.sourcePosition);
      // TODO: find the matching Code log entries.
    }
    this.selectEntries(entries);
  }

  handleSelectEntries(e) {
    e.stopImmediatePropagation();
    this.showEntries(e.entries);
  }

  selectEntries(entries) {
    const missingTypes = App.allEventTypes;
    groupBy(entries, each => each.constructor, true).forEach(group => {
      this.selectEntriesOfSingleType(group.entries);
      missingTypes.delete(group.key);
    });
    missingTypes.forEach(type => this.selectEntriesOfSingleType([], type));
  }

  selectEntriesOfSingleType(entries, type) {
    switch (entries[0]?.constructor ?? type) {
      case SourcePosition:
        return this.showSourcePositions(entries);
      case MapLogEntry:
        return this.showMapEntries(entries);
      case IcLogEntry:
        return this.showIcEntries(entries);
      case ApiLogEntry:
        return this.showApiEntries(entries);
      case CodeLogEntry:
        return this.showCodeEntries(entries);
      case DeoptLogEntry:
        return this.showDeoptEntries(entries);
      default:
        throw new Error('Unknown selection type!');
    }
  }

  showMapEntries(entries) {
    this._state.selectedMapLogEntries = entries;
    this._view.mapPanel.selectedLogEntries = entries;
    this._view.mapList.selectedLogEntries = entries;
  }

  showIcEntries(entries) {
    this._state.selectedIcLogEntries = entries;
    this._view.icList.selectedLogEntries = entries;
  }

  showDeoptEntries(entries) {
    this._state.selectedDeoptLogEntries = entries;
    this._view.deoptList.selectedLogEntries = entries;
  }

  showCodeEntries(entries) {
    this._state.selectedCodeLogEntries = entries;
    this._view.codePanel.selectedEntries = entries;
    this._view.codeList.selectedLogEntries = entries;
  }

  showApiEntries(entries) {
    this._state.selectedApiLogEntries = entries;
    this._view.apiList.selectedLogEntries = entries;
  }

  showSourcePositions(entries) {
    this._view.scriptPanel.selectedSourcePositions = entries
  }

  handleTimeRangeSelect(e) {
    e.stopImmediatePropagation();
    this.selectTimeRange(e.start, e.end);
  }

  selectTimeRange(start, end) {
    this._state.selectTimeRange(start, end);
    this.showMapEntries(this._state.mapTimeline.selectionOrSelf);
    this.showIcEntries(this._state.icTimeline.selectionOrSelf);
    this.showDeoptEntries(this._state.deoptTimeline.selectionOrSelf);
    this.showCodeEntries(this._state.codeTimeline.selectionOrSelf);
    this.showApiEntries(this._state.apiTimeline.selectionOrSelf);
    this._view.timelinePanel.timeSelection = {start, end};
  }

  handleFocusLogEntryl(e) {
    e.stopImmediatePropagation();
    this.focusLogEntry(e.entry);
  }

  focusLogEntry(entry) {
    switch (entry.constructor) {
      case SourcePosition:
        return this.focusSourcePosition(entry);
      case MapLogEntry:
        return this.focusMapLogEntry(entry);
      case IcLogEntry:
        return this.focusIcLogEntry(entry);
      case ApiLogEntry:
        return this.focusApiLogEntry(entry);
      case CodeLogEntry:
        return this.focusCodeLogEntry(entry);
      case DeoptLogEntry:
        return this.focusDeoptLogEntry(entry);
      default:
        throw new Error('Unknown selection type!');
    }
  }

  focusMapLogEntry(entry) {
    this._state.map = entry;
    this._view.mapTrack.focusedEntry = entry;
    this._view.mapPanel.map = entry;
  }

  focusIcLogEntry(entry) {
    this._state.ic = entry;
  }

  focusCodeLogEntry(entry) {
    this._state.code = entry;
    this._view.codePanel.entry = entry;
  }

  focusDeoptLogEntry(entry) {
    this._view.deoptList.focusedLogEntry = entry;
  }

  focusApiLogEntry(entry) {
    this._state.apiLogEntry = entry;
    this._view.apiTrack.focusedEntry = entry;
  }

  focusSourcePosition(sourcePosition) {
    if (!sourcePosition) return;
    this._view.scriptPanel.focusedSourcePositions = [sourcePosition];
  }

  handleToolTip(event) {
    this._view.toolTip.positionOrTargetNode = event.positionOrTargetNode;
    this._view.toolTip.content = event.content;
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
      this._state.profile = processor.profile;
      const mapTimeline = processor.mapTimeline;
      const icTimeline = processor.icTimeline;
      const deoptTimeline = processor.deoptTimeline;
      const codeTimeline = processor.codeTimeline;
      const apiTimeline = processor.apiTimeline;
      this._state.setTimelines(
          mapTimeline, icTimeline, deoptTimeline, codeTimeline, apiTimeline);
      this._view.mapPanel.timeline = mapTimeline;
      this._view.icList.timeline = icTimeline;
      this._view.mapList.timeline = mapTimeline;
      this._view.deoptList.timeline = deoptTimeline;
      this._view.codeList.timeline = codeTimeline;
      this._view.apiList.timeline = apiTimeline;
      this._view.scriptPanel.scripts = processor.scripts;
      this._view.codePanel.timeline = codeTimeline;
      this.refreshTimelineTrackView();
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
    this._view.codeTrack.data = this._state.codeTimeline;
    this._view.apiTrack.data = this._state.apiTimeline;
  }

  switchTheme(event) {
    document.documentElement.dataset.theme =
        event.target.checked ? 'light' : 'dark';
    CSSColor.reset();
    if (!this.fileLoaded) return;
    this.refreshTimelineTrackView();
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
