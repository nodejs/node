// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kChunkHeight, kChunkVisualWidth, kChunkWidth} from '../../log/map.mjs';
import {SelectionEvent, SelectTimeEvent, SynchronizeSelectionEvent, ToolTipEvent,} from '../events.mjs';
import {CSSColor, delay, DOM, formatDurationMicros, V8CustomElement} from '../helper.mjs';

export const kTimelineHeight = 200;

export class TimelineTrackBase extends V8CustomElement {
  _timeline;
  _nofChunks = 500;
  _chunks = [];
  _selectedEntry;
  _focusedEntry;
  _timeToPixel;
  _timeStartPixelOffset;
  _legend;
  _lastContentWidth = 0;

  _cachedTimelineBoundingClientRect;
  _cachedTimelineScrollLeft;

  constructor(templateText) {
    super(templateText);
    this._selectionHandler = new SelectionHandler(this);
    this._legend = new Legend(this.$('#legendTable'));

    this.timelineChunks = this.$('#timelineChunks');
    this.timelineSamples = this.$('#timelineSamples');
    this.timelineNode = this.$('#timeline');
    this._toolTipTargetNode = undefined;
    this.hitPanelNode = this.$('#hitPanel');
    this.timelineAnnotationsNode = this.$('#timelineAnnotations');
    this.timelineMarkersNode = this.$('#timelineMarkers');
    this._scalableContentNode = this.$('#scalableContent');
    this.isLocked = false;
    this.setAttribute('tabindex', 0);
  }

  _initEventListeners() {
    this._legend.onFilter = this._handleFilterTimeline.bind(this);
    this.timelineNode.addEventListener(
        'scroll', this._handleTimelineScroll.bind(this));
    this.hitPanelNode.onclick = this._handleClick.bind(this);
    this.hitPanelNode.ondblclick = this._handleDoubleClick.bind(this);
    this.hitPanelNode.onmousemove = this._handleMouseMove.bind(this);
    this.$('#selectionForeground')
        .addEventListener('mousemove', this._handleMouseMove.bind(this));
    window.addEventListener('resize', () => this._resetCachedDimensions());
  }

  static get observedAttributes() {
    return ['title'];
  }

  attributeChangedCallback(name, oldValue, newValue) {
    if (name == 'title') {
      this.$('#title').innerHTML = newValue;
    }
  }

  _handleFilterTimeline(type) {
    this._updateChunks();
    this._legend.update(true);
  }

  set data(timeline) {
    console.assert(timeline);
    if (!this._timeline) this._initEventListeners();
    this._timeline = timeline;
    this._legend.timeline = timeline;
    this.$('.content').style.display = timeline.isEmpty() ? 'none' : 'relative';
    this._updateChunks();
  }

  set timeSelection({start, end, focus = false, zoom = false}) {
    this._selectionHandler.timeSelection = {start, end};
    this.updateSelection();
    if (focus || zoom) {
      if (!Number.isFinite(start) || !Number.isFinite(end)) {
        throw new Error('Invalid number ranges');
      }
      if (focus) {
        this.currentTime = (start + end) / 2;
      }
      if (zoom) {
        const margin = 0.2;
        const newVisibleTime = (end - start) * (1 + 2 * margin);
        const currentVisibleTime =
            this._cachedTimelineBoundingClientRect.width / this._timeToPixel;
        this.nofChunks = this.nofChunks * (currentVisibleTime / newVisibleTime);
      }
    }
  }

  updateSelection() {
    this._selectionHandler.update();
    this._legend.update();
  }

  get _timelineBoundingClientRect() {
    if (this._cachedTimelineBoundingClientRect === undefined) {
      this._cachedTimelineBoundingClientRect =
          this.timelineNode.getBoundingClientRect();
    }
    return this._cachedTimelineBoundingClientRect;
  }

  get _timelineScrollLeft() {
    if (this._cachedTimelineScrollLeft === undefined) {
      this._cachedTimelineScrollLeft = this.timelineNode.scrollLeft;
    }
    return this._cachedTimelineScrollLeft;
  }

  _resetCachedDimensions() {
    this._cachedTimelineBoundingClientRect = undefined;
    this._cachedTimelineScrollLeft = undefined;
  }

  // Maps the clicked x position to the x position on timeline
  positionOnTimeline(pagePosX) {
    let rect = this._timelineBoundingClientRect;
    let posClickedX = pagePosX - rect.left + this._timelineScrollLeft;
    return posClickedX;
  }

  positionToTime(pagePosX) {
    return this.relativePositionToTime(this.positionOnTimeline(pagePosX));
  }

  relativePositionToTime(timelineRelativeX) {
    const timelineAbsoluteX = timelineRelativeX + this._timeStartPixelOffset;
    return (timelineAbsoluteX / this._timeToPixel) | 0;
  }

  timeToPosition(time) {
    let relativePosX = time * this._timeToPixel;
    relativePosX -= this._timeStartPixelOffset;
    return relativePosX;
  }

  set nofChunks(count) {
    const centerTime = this.currentTime;
    const kMinNofChunks = 100;
    if (count < kMinNofChunks) count = kMinNofChunks;
    const kMaxNofChunks = 10 * 1000;
    if (count > kMaxNofChunks) count = kMaxNofChunks;
    this._nofChunks = count | 0;
    this._updateChunks();
    this.currentTime = centerTime;
  }

  get nofChunks() {
    return this._nofChunks;
  }

  _updateChunks() {
    this._chunks = undefined;
    this._updateDimensions();
    this.requestUpdate();
  }

  get chunks() {
    if (this._chunks?.length != this.nofChunks) {
      this._chunks =
          this._timeline.chunks(this.nofChunks, this._legend.filterPredicate);
      console.assert(this._chunks.length == this._nofChunks);
    }
    return this._chunks;
  }

  set selectedEntry(value) {
    this._selectedEntry = value;
  }

  get selectedEntry() {
    return this._selectedEntry;
  }

  get focusedEntry() {
    return this._focusedEntry;
  }

  set focusedEntry(entry) {
    this._focusedEntry = entry;
    if (entry) this._drawAnnotations(entry);
  }

  set scrollLeft(offset) {
    this.timelineNode.scrollLeft = offset;
    this._cachedTimelineScrollLeft = offset;
  }

  get scrollLeft() {
    return this._cachedTimelineScrollLeft;
  }

  set currentTime(time) {
    const position = this.timeToPosition(time);
    const centerOffset = this._timelineBoundingClientRect.width / 2;
    this.scrollLeft = Math.max(0, position - centerOffset);
  }

  get currentTime() {
    const centerOffset =
        this._timelineBoundingClientRect.width / 2 + this.scrollLeft;
    return this.relativePositionToTime(centerOffset);
  }

  handleEntryTypeDoubleClick(e) {
    this.dispatchEvent(new SelectionEvent(e.target.parentNode.entries));
  }

  timelineIndicatorMove(offset) {
    this.timelineNode.scrollLeft += offset;
    this._cachedTimelineScrollLeft = undefined;
  }

  _handleTimelineScroll(e) {
    let scrollLeft = e.currentTarget.scrollLeft;
    this._cachedTimelineScrollLeft = scrollLeft;
    this.dispatchEvent(new CustomEvent(
        'scrolltrack', {bubbles: true, composed: true, detail: scrollLeft}));
  }

  _updateDimensions() {
    // No data in this timeline, no need to resize
    if (!this._timeline) return;

    const centerOffset = this._timelineBoundingClientRect.width / 2;
    const time =
        this.relativePositionToTime(this._timelineScrollLeft + centerOffset);
    const start = this._timeline.startTime;
    const width = this._nofChunks * kChunkWidth;
    this._lastContentWidth = parseInt(this.timelineMarkersNode.style.width);
    this._timeToPixel = width / this._timeline.duration();
    this._timeStartPixelOffset = start * this._timeToPixel;
    this.$('#cropper').style.width = `${width}px`;
    this.timelineMarkersNode.style.width = `${width}px`;
    this.timelineAnnotationsNode.style.width = `${width}px`;
    this.hitPanelNode.style.width = `${width}px`;

    const ratio = this._scaleContent(width) || 1;
    this.timelineChunks.style.width = `${width / Math.min(1, ratio)}px`;
    this._drawMarkers();
    this._selectionHandler.update();

    this._cachedTimelineScrollLeft = this.timelineNode.scrollLeft =
        this.timeToPosition(time) - centerOffset;
  }

  _scaleContent(currentWidth) {
    if (!this._lastContentWidth) return 1;
    const ratio = currentWidth / this._lastContentWidth;
    this._scalableContentNode.style.transform = `scale(${ratio}, 1)`;
    return ratio;
  }

  _adjustHeight(height) {
    const dataHeight = Math.max(height, 200);
    const viewHeight = Math.min(dataHeight, 400);
    this.style.setProperty('--data-height', dataHeight + 'px');
    this.style.setProperty('--view-height', viewHeight + 'px');
    this.timelineNode.style.overflowY =
        (height > kTimelineHeight) ? 'scroll' : 'hidden';
  }

  _update() {
    this._legend.update();
    this._drawContent().then(() => this._drawAnnotations(this.selectedEntry));
    this._resetCachedDimensions();
  }

  async _drawContent() {
    if (this._timeline.isEmpty()) return;
    await delay(5);
    const chunks = this.chunks;
    const max = chunks.max(each => each.size());
    let buffer = '';
    for (let i = 0; i < chunks.length; i++) {
      const chunk = chunks[i];
      const height = (chunk.size() / max * kChunkHeight);
      chunk.height = height;
      if (chunk.isEmpty()) continue;
      buffer += '<g>';
      buffer += this._drawChunk(i, chunk);
      buffer += '</g>'
    }
    this._scalableContentNode.innerHTML = buffer;
    this._scalableContentNode.style.transform = 'scale(1, 1)';
  }

  _drawChunk(chunkIndex, chunk) {
    const groups = chunk.getBreakdown(event => event.type);
    let buffer = '';
    const kHeight = chunk.height;
    let lastHeight = kTimelineHeight;
    for (let i = 0; i < groups.length; i++) {
      const group = groups[i];
      if (group.length == 0) break;
      const height = (group.length / chunk.size() * kHeight) | 0;
      lastHeight -= height;
      const color = this._legend.colorForType(group.key);
      buffer += `<rect x=${chunkIndex * kChunkWidth} y=${lastHeight} height=${
          height} width=${kChunkVisualWidth} fill=${color} />`
    }
    return buffer;
  }

  _drawMarkers() {
    // Put a time marker roughly every 20 chunks.
    const expected = this._timeline.duration() / this._nofChunks * 20;
    let interval = (10 ** Math.floor(Math.log10(expected)));
    let correction = Math.log10(expected / interval);
    correction = (correction < 0.33) ? 1 : (correction < 0.75) ? 2.5 : 5;
    interval *= correction;

    const start = this._timeline.startTime;
    let time = start;
    let buffer = '';
    while (time < this._timeline.endTime) {
      const delta = time - start;
      const text = `${(delta / 1000) | 0} ms`;
      const x = (delta * this._timeToPixel) | 0;
      buffer += `<text x=${x - 2} y=0 class=markerText >${text}</text>`
      buffer +=
          `<line x1=${x} x2=${x} y1=12 y2=2000 dy=100% class=markerLine />`
      time += interval;
    }
    this.timelineMarkersNode.innerHTML = buffer;
  }

  _drawAnnotations(logEntry, time) {
    if (!this._focusedEntry) return;
    this._drawEntryMark(this._focusedEntry);
  }

  _drawEntryMark(entry) {
    const [x, y] = this._positionForEntry(entry);
    const color = this._legend.colorForType(entry.type);
    const mark =
        `<circle cx=${x} cy=${y} r=3 stroke=${color} class=annotationPoint />`;
    this.timelineAnnotationsNode.innerHTML = mark;
  }

  _handleUnlockedMouseEvent(event) {
    this._focusedEntry = this._getEntryForEvent(event);
    if (!this._focusedEntry) return false;
    this._updateToolTip(event);
    const time = this.positionToTime(event.pageX);
    this._drawAnnotations(this._focusedEntry, time);
  }

  _updateToolTip(event) {
    if (!this._focusedEntry) return false;
    this.dispatchEvent(new ToolTipEvent(
        this._focusedEntry, this._toolTipTargetNode, event.ctrlKey));
    event.stopImmediatePropagation();
  }

  _handleClick(event) {
    if (event.button !== 0) return;
    if (event.target === this.timelineChunks) return;
    this.isLocked = !this.isLocked;
    // Do this unconditionally since we want the tooltip to be update to the
    // latest locked state.
    this._handleUnlockedMouseEvent(event);
    return false;
  }

  _handleDoubleClick(event) {
    if (event.button !== 0) return;
    this._selectionHandler.clearSelection();
    const time = this.positionToTime(event.pageX);
    const chunk = this._getChunkForEvent(event)
    if (!chunk) return;
    event.stopImmediatePropagation();
    this.dispatchEvent(new SelectTimeEvent(chunk.start, chunk.end));
    return false;
  }

  _handleMouseMove(event) {
    if (event.button !== 0) return;
    if (this._selectionHandler.isSelecting) return false;
    if (this.isLocked && this._focusedEntry) {
      this._updateToolTip(event);
      return false;
    }
    this._handleUnlockedMouseEvent(event);
  }

  _getChunkForEvent(event) {
    const time = this.positionToTime(event.pageX);
    return this._chunkForTime(time);
  }

  _chunkForTime(time) {
    const chunkIndex = ((time - this._timeline.startTime) /
                        this._timeline.duration() * this._nofChunks) |
        0;
    return this.chunks[chunkIndex];
  }

  _positionForEntry(entry) {
    const chunk = this._chunkForTime(entry.time);
    if (chunk === undefined) return [-1, -1];
    const xFrom = (chunk.index * kChunkWidth + kChunkVisualWidth / 2) | 0;
    const yFrom = kTimelineHeight - chunk.yOffset(entry) | 0;
    return [xFrom, yFrom];
  }

  _getEntryForEvent(event) {
    const chunk = this._getChunkForEvent(event);
    if (chunk?.isEmpty() ?? true) return false;
    const relativeIndex = Math.round(
        (kTimelineHeight - event.layerY) / chunk.height * (chunk.size() - 1));
    if (relativeIndex > chunk.size()) return false;
    const logEntry = chunk.at(relativeIndex);
    const node = this.getToolTipTargetNode(logEntry);
    if (!node) return logEntry;
    const style = node.style;
    style.left = `${chunk.index * kChunkWidth}px`;
    style.top = `${kTimelineHeight - chunk.height}px`;
    style.height = `${chunk.height}px`;
    style.width = `${kChunkVisualWidth}px`;
    return logEntry;
  }

  getToolTipTargetNode(logEntry) {
    let node = this._toolTipTargetNode;
    if (node) {
      if (node.logEntry === logEntry) return undefined;
      node.parentNode.removeChild(node);
    }
    node = this._toolTipTargetNode = DOM.div('toolTipTarget');
    node.logEntry = logEntry;
    this.$('#cropper').appendChild(node);
    return node;
  }
};

class SelectionHandler {
  // TODO turn into static field once Safari supports it.
  static get SELECTION_OFFSET() {
    return 10
  };

  _timeSelection = {start: -1, end: Infinity};
  _selectionOriginTime = -1;

  constructor(timeline) {
    this._timeline = timeline;
    this._timelineNode = this._timeline.$('#timeline');
    this._timelineNode.addEventListener(
        'mousedown', this._handleMouseDown.bind(this));
    this._timelineNode.addEventListener(
        'mouseup', this._handleMouseUp.bind(this));
    this._timelineNode.addEventListener(
        'mousemove', this._handleMouseMove.bind(this));
    this._selectionNode = this._timeline.$('#selection');
    this._selectionForegroundNode = this._timeline.$('#selectionForeground');
    this._selectionForegroundNode.addEventListener(
        'dblclick', this._handleDoubleClick.bind(this));
    this._selectionBackgroundNode = this._timeline.$('#selectionBackground');
    this._leftHandleNode = this._timeline.$('#leftHandle');
    this._rightHandleNode = this._timeline.$('#rightHandle');
  }

  update() {
    if (!this.hasSelection) {
      this._selectionNode.style.display = 'none';
      return;
    }
    this._selectionNode.style.display = 'inherit';
    const startPosition = this.timeToPosition(this._timeSelection.start);
    const endPosition = this.timeToPosition(this._timeSelection.end);
    this._leftHandleNode.style.left = startPosition + 'px';
    this._rightHandleNode.style.left = endPosition + 'px';
    const delta = endPosition - startPosition;
    this._selectionForegroundNode.style.left = startPosition + 'px';
    this._selectionForegroundNode.style.width = delta + 'px';
    this._selectionBackgroundNode.style.left = startPosition + 'px';
    this._selectionBackgroundNode.style.width = delta + 'px';
  }

  set timeSelection(selection) {
    this._timeSelection.start = selection.start;
    this._timeSelection.end = selection.end;
  }

  clearSelection() {
    this._timeline.dispatchEvent(new SelectTimeEvent());
  }

  timeToPosition(posX) {
    return this._timeline.timeToPosition(posX);
  }

  positionToTime(posX) {
    return this._timeline.positionToTime(posX);
  }

  get isSelecting() {
    return this._selectionOriginTime >= 0;
  }

  get hasSelection() {
    return this._timeSelection.start >= 0 &&
        this._timeSelection.end != Infinity;
  }

  get _leftHandlePosX() {
    return this._leftHandleNode.getBoundingClientRect().x;
  }

  get _rightHandlePosX() {
    return this._rightHandleNode.getBoundingClientRect().x;
  }

  _isOnLeftHandle(posX) {
    return Math.abs(this._leftHandlePosX - posX) <=
        SelectionHandler.SELECTION_OFFSET;
  }

  _isOnRightHandle(posX) {
    return Math.abs(this._rightHandlePosX - posX) <=
        SelectionHandler.SELECTION_OFFSET;
  }

  _handleMouseDown(event) {
    if (event.button !== 0) return;
    let xPosition = event.clientX
    // Update origin time in case we click on a handle.
    if (this._isOnLeftHandle(xPosition)) {
      xPosition = this._rightHandlePosX;
    }
    else if (this._isOnRightHandle(xPosition)) {
      xPosition = this._leftHandlePosX;
    }
    this._selectionOriginTime = this.positionToTime(xPosition);
  }

  _handleMouseMove(event) {
    if (event.button !== 0) return;
    if (!this.isSelecting) return;
    const currentTime = this.positionToTime(event.clientX);
    this._timeline.dispatchEvent(new SynchronizeSelectionEvent(
        Math.min(this._selectionOriginTime, currentTime),
        Math.max(this._selectionOriginTime, currentTime)));
  }

  _handleMouseUp(event) {
    if (event.button !== 0) return;
    this._selectionOriginTime = -1;
    if (this._timeSelection.start === -1) return;
    const delta = this._timeSelection.end - this._timeSelection.start;
    if (delta <= 1 || isNaN(delta)) return;
    this._timeline.dispatchEvent(new SelectTimeEvent(
        this._timeSelection.start, this._timeSelection.end));
  }

  _handleDoubleClick(event) {
    if (!this.hasSelection) return;
    // Focus and zoom to the current selection.
    this._timeline.dispatchEvent(new SelectTimeEvent(
        this._timeSelection.start, this._timeSelection.end, true, true));
  }
}

class Legend {
  _timeline;
  _lastSelection;
  _typesFilters = new Map();
  _typeClickHandler = this._handleTypeClick.bind(this);
  _filterPredicate = this.filter.bind(this);
  onFilter = () => {};

  constructor(table) {
    this._table = table;
    this._enableDuration = false;
  }

  set timeline(timeline) {
    this._timeline = timeline;
    const groups = timeline.getBreakdown();
    this._typesFilters = new Map(groups.map(each => [each.key, true]));
    this._colors =
        new Map(groups.map(each => [each.key, CSSColor.at(each.id)]));
  }

  get selection() {
    return this._timeline.selectionOrSelf;
  }

  get filterPredicate() {
    for (let visible of this._typesFilters.values()) {
      if (!visible) return this._filterPredicate;
    }
    return undefined;
  }

  colorForType(type) {
    let color = this._colors.get(type);
    if (color === undefined) {
      color = CSSColor.at(this._colors.size);
      this._colors.set(type, color);
    }
    return color;
  }

  filter(logEntry) {
    return this._typesFilters.get(logEntry.type);
  }

  update(force = false) {
    if (!force && this._lastSelection === this.selection) return;
    this._lastSelection = this.selection;
    const tbody = DOM.tbody();
    const missingTypes = new Set(this._typesFilters.keys());
    this._checkDurationField();
    let selectionDuration = 0;
    const breakdown =
        this.selection.getBreakdown(undefined, this._enableDuration);
    if (this._enableDuration) {
      if (this.selection.cachedDuration === undefined) {
        this.selection.cachedDuration = this._breakdownTotalDuration(breakdown);
      }
      selectionDuration = this.selection.cachedDuration;
    }
    breakdown.forEach(group => {
      tbody.appendChild(this._addTypeRow(group, selectionDuration));
      missingTypes.delete(group.key);
    });
    missingTypes.forEach(key => {
      const emptyGroup = {key, length: 0, duration: 0};
      tbody.appendChild(this._addTypeRow(emptyGroup, selectionDuration));
    });
    if (this._timeline.selection) {
      tbody.appendChild(this._addRow(
          '', 'Selection', this.selection.length, '100%', selectionDuration,
          '100%'));
    }
    // Showing 100% for 'All' and for 'Selection' would be confusing.
    const allPercent = this._timeline.selection ? '' : '100%';
    tbody.appendChild(this._addRow(
        '', 'All', this._timeline.length, allPercent,
        this._timeline.cachedDuration, allPercent));
    this._table.tBodies[0].replaceWith(tbody);
  }

  _checkDurationField() {
    if (this._enableDuration) return;
    const example = this.selection.at(0);
    if (!example || !('duration' in example)) return;
    this._enableDuration = true;
    this._table.tHead.rows[0].appendChild(DOM.td('Duration'));
  }

  _addRow(colorNode, type, count, countPercent, duration, durationPercent) {
    const row = DOM.tr();
    const colorCell = row.appendChild(DOM.td(colorNode, 'color'));
    colorCell.setAttribute('title', `Toggle '${type}' entries.`);
    const typeCell = row.appendChild(DOM.td(type, 'text'));
    typeCell.setAttribute('title', type);
    row.appendChild(DOM.td(count.toString()));
    row.appendChild(DOM.td(countPercent));
    if (this._enableDuration) {
      row.appendChild(DOM.td(formatDurationMicros(duration ?? 0)));
      row.appendChild(DOM.td(durationPercent ?? '0%'));
    }
    return row
  }

  _addTypeRow(group, selectionDuration) {
    const color = this.colorForType(group.key);
    const classes = ['colorbox'];
    if (group.length == 0) classes.push('empty');
    const colorDiv = DOM.div(classes);
    colorDiv.style.borderColor = color;
    if (this._typesFilters.get(group.key)) {
      colorDiv.style.backgroundColor = color;
    } else {
      colorDiv.style.backgroundColor = CSSColor.backgroundImage;
    }
    let duration = 0;
    let durationPercent = '';
    if (this._enableDuration) {
      // group.duration was added in _breakdownTotalDuration.
      duration = group.duration;
      durationPercent = selectionDuration == 0 ?
          '0%' :
          this._formatPercent(duration / selectionDuration);
    }
    const countPercent =
        this._formatPercent(group.length / this.selection.length);
    const row = this._addRow(
        colorDiv, group.key, group.length, countPercent, duration,
        durationPercent);
    row.className = 'clickable';
    row.onclick = this._typeClickHandler;
    row.data = group.key;
    return row;
  }

  _handleTypeClick(e) {
    const type = e.currentTarget.data;
    this._typesFilters.set(type, !this._typesFilters.get(type));
    this.onFilter(type);
  }

  _breakdownTotalDuration(breakdown) {
    let duration = 0;
    breakdown.forEach(group => {
      group.duration = this._groupDuration(group);
      duration += group.duration;
    })
    return duration;
  }

  _groupDuration(group) {
    let duration = 0;
    const entries = group.entries;
    for (let i = 0; i < entries.length; i++) {
      duration += entries[i].duration;
    }
    return duration;
  }

  _formatPercent(ratio) {
    return `${(ratio * 100).toFixed(1)}%`;
  }
}
