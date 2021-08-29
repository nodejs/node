// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Script, SourcePosition} from '../profile.mjs';

import {State} from './app-model.mjs';
import {ApiLogEntry} from './log/api.mjs';
import {CodeLogEntry} from './log/code.mjs';
import {DeoptLogEntry} from './log/code.mjs';
import {SharedLibLogEntry} from './log/code.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {LogEntry} from './log/log.mjs';
import {MapLogEntry} from './log/map.mjs';
import {TickLogEntry} from './log/tick.mjs';
import {TimerLogEntry} from './log/timer.mjs';
import {Processor} from './processor.mjs';
import {Timeline} from './timeline.mjs'
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
      tickTrack: $('#tick-track'),
      mapTrack: $('#map-track'),
      icTrack: $('#ic-track'),
      deoptTrack: $('#deopt-track'),
      codeTrack: $('#code-track'),
      apiTrack: $('#api-track'),
      timerTrack: $('#timer-track'),

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
    this._view.logFileReader.addEventListener(
        'fileuploadstart', (e) => this.handleFileUploadStart(e));
    this._view.logFileReader.addEventListener(
        'fileuploadchunk', (e) => this.handleFileUploadChunk(e));
    this._view.logFileReader.addEventListener(
        'fileuploadend', (e) => this.handleFileUploadEnd(e));
    this._startupPromise = this._loadCustomElements();
    this._view.codeTrack.svg = true;
  }

  static get allEventTypes() {
    return new Set([
      SourcePosition,
      MapLogEntry,
      IcLogEntry,
      ApiLogEntry,
      CodeLogEntry,
      DeoptLogEntry,
      SharedLibLogEntry,
      TickLogEntry,
      TimerLogEntry,
    ]);
  }

  async _loadCustomElements() {
    await Promise.all([
      import('./view/list-panel.mjs'),
      import('./view/timeline-panel.mjs'),
      import('./view/map-panel.mjs'),
      import('./view/script-panel.mjs'),
      import('./view/code-panel.mjs'),
      import('./view/property-link-table.mjs'),
      import('./view/tool-tip.mjs'),
    ]);
    this._addEventListeners();
  }

  _addEventListeners() {
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
      case DeoptLogEntry:
        // TODO select map + code entries
        if (entry.fileSourcePosition) entries.push(entry.fileSourcePosition);
        break;
      case Script:
        entries = entry.entries.concat(entry.sourcePositions);
        break;
      case TimerLogEntry:
      case ApiLogEntry:
      case CodeLogEntry:
      case TickLogEntry:
      case SharedLibLogEntry:
        break;
      default:
        throw new Error(`Unknown selection type: ${entry.constructor?.name}`);
    }
    if (entry.sourcePosition) {
      entries.push(entry.sourcePosition);
      // TODO: find the matching Code log entries.
    }
    this.selectEntries(entries);
  }

  static isClickable(object) {
    if (typeof object !== 'object') return false;
    if (object instanceof LogEntry) return true;
    if (object instanceof SourcePosition) return true;
    if (object instanceof Script) return true;
    return false;
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
    const entryType = entries[0]?.constructor ?? type;
    switch (entryType) {
      case Script:
        entries = entries.flatMap(script => script.sourcePositions);
        return this.showSourcePositions(entries);
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
      case SharedLibLogEntry:
        return this.showSharedLibEntries(entries);
      case TimerLogEntry:
      case TickLogEntry:
        break;
      default:
        throw new Error(`Unknown selection type: ${entryType?.name}`);
    }
  }

  showMapEntries(entries, focusView = true) {
    this._view.mapPanel.selectedLogEntries = entries;
    this._view.mapList.selectedLogEntries = entries;
    if (focusView) this._view.mapPanel.show();
  }

  showIcEntries(entries, focusView = true) {
    this._view.icList.selectedLogEntries = entries;
    if (focusView) this._view.icList.show();
  }

  showDeoptEntries(entries, focusView = true) {
    this._view.deoptList.selectedLogEntries = entries;
    if (focusView) this._view.deoptList.show();
  }

  showSharedLibEntries(entries, focusView = true) {}

  showCodeEntries(entries, focusView = true) {
    this._view.codePanel.selectedEntries = entries;
    this._view.codeList.selectedLogEntries = entries;
    if (focusView) this._view.codePanel.show();
  }

  showApiEntries(entries, focusView = true) {
    this._view.apiList.selectedLogEntries = entries;
    if (focusView) this._view.apiList.show();
  }

  showTickEntries(entries, focusView = true) {}
  showTimerEntries(entries, focusView = true) {}

  showSourcePositions(entries, focusView = true) {
    this._view.scriptPanel.selectedSourcePositions = entries
    if (focusView) this._view.scriptPanel.show();
  }

  handleTimeRangeSelect(e) {
    e.stopImmediatePropagation();
    this.selectTimeRange(e.start, e.end);
  }

  selectTimeRange(start, end) {
    this._state.selectTimeRange(start, end);
    this.showMapEntries(this._state.mapTimeline.selectionOrSelf, false);
    this.showIcEntries(this._state.icTimeline.selectionOrSelf, false);
    this.showDeoptEntries(this._state.deoptTimeline.selectionOrSelf, false);
    this.showCodeEntries(this._state.codeTimeline.selectionOrSelf, false);
    this.showApiEntries(this._state.apiTimeline.selectionOrSelf, false);
    this.showTickEntries(this._state.tickTimeline.selectionOrSelf, false);
    this.showTimerEntries(this._state.timerTimeline.selectionOrSelf, false);
    this._view.timelinePanel.timeSelection = {start, end};
  }

  handleFocusLogEntryl(e) {
    e.stopImmediatePropagation();
    this.focusLogEntry(e.entry);
  }

  focusLogEntry(entry) {
    switch (entry.constructor) {
      case Script:
        return this.focusSourcePosition(entry.sourcePositions[0]);
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
      case SharedLibLogEntry:
        return this.focusDeoptLogEntry(entry);
      case TickLogEntry:
        return this.focusTickLogEntry(entry);
      case TimerLogEntry:
        return this.focusTimerLogEntry(entry);
      default:
        throw new Error(`Unknown selection type: ${entry.constructor?.name}`);
    }
  }

  focusMapLogEntry(entry, focusSourcePosition = true) {
    this._state.map = entry;
    this._view.mapTrack.focusedEntry = entry;
    this._view.mapPanel.map = entry;
    this._view.mapPanel.show();
    if (focusSourcePosition) {
      this.focusCodeLogEntry(entry.code, false);
      this.focusSourcePosition(entry.sourcePosition);
    }
  }

  focusIcLogEntry(entry) {
    this._state.ic = entry;
    this.focusMapLogEntry(entry.map, false);
    this.focusCodeLogEntry(entry.code, false);
    this.focusSourcePosition(entry.sourcePosition);
  }

  focusCodeLogEntry(entry, focusSourcePosition = true) {
    this._state.code = entry;
    this._view.codePanel.entry = entry;
    if (focusSourcePosition) this.focusSourcePosition(entry.sourcePosition);
    this._view.codePanel.show();
  }

  focusDeoptLogEntry(entry) {
    this._state.deoptLogEntry = entry;
    this.focusCodeLogEntry(entry.code, false);
    this.focusSourcePosition(entry.sourcePosition);
  }

  focusSharedLibLogEntry(entry) {
    // no-op.
  }

  focusApiLogEntry(entry) {
    this._state.apiLogEntry = entry;
    this._view.apiTrack.focusedEntry = entry;
    this.focusSourcePosition(entry.sourcePosition);
  }

  focusTickLogEntry(entry) {
    this._state.tickLogEntry = entry;
    this._view.tickTrack.focusedEntry = entry;
  }

  focusTimerLogEntry(entry) {
    this._state.timerLogEntry = entry;
    this._view.timerTrack.focusedEntry = entry;
  }

  focusSourcePosition(sourcePosition) {
    if (!sourcePosition) return;
    this._view.scriptPanel.focusedSourcePositions = [sourcePosition];
    this._view.scriptPanel.show();
  }

  handleToolTip(event) {
    let content = event.content;
    if (typeof content !== 'string' &&
        !(content?.nodeType && content?.nodeName)) {
      content = content?.toolTipDict;
    }
    if (!content) {
      throw new Error(
          `Unknown tooltip content type: ${content.constructor?.name}`);
    }
    this.setToolTip(content, event.positionOrTargetNode);
  }

  setToolTip(content, positionOrTargetNode) {
    this._view.toolTip.positionOrTargetNode = positionOrTargetNode;
    this._view.toolTip.content = content;
  }

  restartApp() {
    this._state = new State();
    this._navigation = new Navigation(this._state, this._view);
  }

  handleFileUploadStart(e) {
    this.restartApp();
    $('#container').className = 'initial';
    this._processor = new Processor();
  }

  async handleFileUploadChunk(e) {
    this._processor.processChunk(e.detail);
  }

  async handleFileUploadEnd(e) {
    try {
      const processor = this._processor;
      await processor.finalize();
      await this._startupPromise;

      this._state.profile = processor.profile;
      const mapTimeline = processor.mapTimeline;
      const icTimeline = processor.icTimeline;
      const deoptTimeline = processor.deoptTimeline;
      const codeTimeline = processor.codeTimeline;
      const apiTimeline = processor.apiTimeline;
      const tickTimeline = processor.tickTimeline;
      const timerTimeline = processor.timerTimeline;
      this._state.setTimelines(
          mapTimeline, icTimeline, deoptTimeline, codeTimeline, apiTimeline,
          tickTimeline, timerTimeline);
      this._view.mapPanel.timeline = mapTimeline;
      this._view.icList.timeline = icTimeline;
      this._view.mapList.timeline = mapTimeline;
      this._view.deoptList.timeline = deoptTimeline;
      this._view.codeList.timeline = codeTimeline;
      this._view.apiList.timeline = apiTimeline;
      this._view.scriptPanel.scripts = processor.scripts;
      this._view.codePanel.timeline = codeTimeline;
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
    this._view.tickTrack.data = this._state.tickTimeline;
    this._view.timerTrack.data = this._state.timerTimeline;
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
    if (!this.map.parent) return;
    this.map = this.map.parent;
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
