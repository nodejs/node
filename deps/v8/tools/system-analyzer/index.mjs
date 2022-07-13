// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Script, SourcePosition} from '../profile.mjs';

import {State} from './app-model.mjs';
import {CodeLogEntry} from './log/code.mjs';
import {DeoptLogEntry} from './log/code.mjs';
import {SharedLibLogEntry} from './log/code.mjs';
import {IcLogEntry} from './log/ic.mjs';
import {LogEntry} from './log/log.mjs';
import {MapLogEntry} from './log/map.mjs';
import {TickLogEntry} from './log/tick.mjs';
import {TimerLogEntry} from './log/timer.mjs';
import {Processor} from './processor.mjs';
import {FocusEvent, SelectionEvent, SelectRelatedEvent, SelectTimeEvent, ToolTipEvent,} from './view/events.mjs';
import {$, groupBy} from './view/helper.mjs';

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
      timerTrack: $('#timer-track'),

      icList: $('#ic-list'),
      mapList: $('#map-list'),
      codeList: $('#code-list'),
      deoptList: $('#deopt-list'),

      mapPanel: $('#map-panel'),
      codePanel: $('#code-panel'),
      scriptPanel: $('#script-panel'),

      toolTip: $('#tool-tip'),
    };
    this._view.logFileReader.addEventListener(
        'fileuploadstart', this.handleFileUploadStart.bind(this));
    this._view.logFileReader.addEventListener(
        'fileuploadchunk', this.handleFileUploadChunk.bind(this));
    this._view.logFileReader.addEventListener(
        'fileuploadend', this.handleFileUploadEnd.bind(this));
    this._startupPromise = this._loadCustomElements();
    this._view.codeTrack.svg = true;
  }

  static get allEventTypes() {
    return new Set([
      SourcePosition,
      MapLogEntry,
      IcLogEntry,
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
        SelectRelatedEvent.name, this.handleSelectRelatedEntries.bind(this));
    document.addEventListener(
        SelectionEvent.name, this.handleSelectEntries.bind(this))
    document.addEventListener(
        FocusEvent.name, this.handleFocusLogEntry.bind(this));
    document.addEventListener(
        SelectTimeEvent.name, this.handleTimeRangeSelect.bind(this));
    document.addEventListener(ToolTipEvent.name, this.handleToolTip.bind(this));
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
    this.selectEntries(e.entries);
  }

  selectEntries(entries) {
    const missingTypes = App.allEventTypes;
    groupBy(entries, each => each.constructor, true).forEach(group => {
      this.selectEntriesOfSingleType(group.entries);
      missingTypes.delete(group.key);
    });
    missingTypes.forEach(
        type => this.selectEntriesOfSingleType([], type, false));
  }

  selectEntriesOfSingleType(entries, type, focusView = true) {
    const entryType = entries[0]?.constructor ?? type;
    switch (entryType) {
      case Script:
        entries = entries.flatMap(script => script.sourcePositions);
        return this.showSourcePositions(entries, focusView);
      case SourcePosition:
        return this.showSourcePositions(entries, focusView);
      case MapLogEntry:
        return this.showMapEntries(entries, focusView);
      case IcLogEntry:
        return this.showIcEntries(entries, focusView);
      case CodeLogEntry:
        return this.showCodeEntries(entries, focusView);
      case DeoptLogEntry:
        return this.showDeoptEntries(entries, focusView);
      case SharedLibLogEntry:
        return this.showSharedLibEntries(entries, focusView);
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

  showTickEntries(entries, focusView = true) {}
  showTimerEntries(entries, focusView = true) {}

  showSourcePositions(entries, focusView = true) {
    this._view.scriptPanel.selectedSourcePositions = entries
    if (focusView) this._view.scriptPanel.show();
  }

  handleTimeRangeSelect(e) {
    e.stopImmediatePropagation();
    this.selectTimeRange(e.start, e.end, e.focus, e.zoom);
  }

  selectTimeRange(start, end, focus = false, zoom = false) {
    this._state.selectTimeRange(start, end);
    this.showMapEntries(this._state.mapTimeline.selectionOrSelf, false);
    this.showIcEntries(this._state.icTimeline.selectionOrSelf, false);
    this.showDeoptEntries(this._state.deoptTimeline.selectionOrSelf, false);
    this.showCodeEntries(this._state.codeTimeline.selectionOrSelf, false);
    this.showTickEntries(this._state.tickTimeline.selectionOrSelf, false);
    this.showTimerEntries(this._state.timerTimeline.selectionOrSelf, false);
    this._view.timelinePanel.timeSelection = {start, end, focus, zoom};
  }

  handleFocusLogEntry(e) {
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
    if (focusSourcePosition) {
      this.focusCodeLogEntry(entry.code, false);
      this.focusSourcePosition(entry.sourcePosition);
    }
    this._view.mapPanel.show();
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
    this._processor.setProgressCallback(
        e.detail.totalSize, e.detail.progressCallback);
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
      const tickTimeline = processor.tickTimeline;
      const timerTimeline = processor.timerTimeline;
      this._state.setTimelines(
          mapTimeline, icTimeline, deoptTimeline, codeTimeline, tickTimeline,
          timerTimeline);
      this._view.mapPanel.timeline = mapTimeline;
      this._view.icList.timeline = icTimeline;
      this._view.mapList.timeline = mapTimeline;
      this._view.deoptList.timeline = deoptTimeline;
      this._view.codeList.timeline = codeTimeline;
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

  updateUrl() {
    let entries = this.state.entries;
    let params = new URLSearchParams(entries);
    window.history.pushState(entries, '', '?' + params.toString());
  }

  scrollLeft() {}

  scrollRight() {}

  handleKeyDown(event) {
    switch (event.key) {
      case 'd':
        this.scrollLeft();
        return false;
      case 'a':
        this.scrollRight();
        return false;
      case '+':
        this.increaseTimelineResolution();
        return false;
      case '-':
        this.decreaseTimelineResolution();
        return false;
    }
  }
}

export {App};
