// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {delay} from '../../helper.mjs';
import {kChunkHeight, kChunkVisualWidth, kChunkWidth} from '../../log/map.mjs';
import {SelectionEvent, SelectTimeEvent, SynchronizeSelectionEvent, ToolTipEvent,} from '../events.mjs';
import {CSSColor, DOM, formatDurationMicros, SVG, V8CustomElement} from '../helper.mjs';

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
    this._legend.onFilter = (type) => this._handleFilterTimeline();

    this.timelineChunks = this.$('#timelineChunks');
    this.timelineSamples = this.$('#timelineSamples');
    this.timelineNode = this.$('#timeline');
    this.toolTipTargetNode = this.$('#toolTipTarget');
    this.hitPanelNode = this.$('#hitPanel');
    this.timelineAnnotationsNode = this.$('#timelineAnnotations');
    this.timelineMarkersNode = this.$('#timelineMarkers');
    this._scalableContentNode = this.$('#scalableContent');

    this.timelineNode.addEventListener(
        'scroll', e => this._handleTimelineScroll(e));
    this.hitPanelNode.onclick = this._handleClick.bind(this);
    this.hitPanelNode.ondblclick = this._handleDoubleClick.bind(this);
    this.hitPanelNode.onmousemove = this._handleMouseMove.bind(this);
    window.addEventListener('resize', () => this._resetCachedDimensions());
    this.isLocked = false;
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
  }

  set data(timeline) {
    this._timeline = timeline;
    this._legend.timeline = timeline;
    this.$('.content').style.display = timeline.isEmpty() ? 'none' : 'relative';
    this._updateChunks();
  }

  set timeSelection(selection) {
    this._selectionHandler.timeSelection = selection;
    this.updateSelection();
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
    this._nofChunks = count | 0;
    this._updateChunks();
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
    return this._chunks;
  }

  set selectedEntry(value) {
    this._selectedEntry = value;
    this.drawAnnotations(value);
  }

  get selectedEntry() {
    return this._selectedEntry;
  }

  set scrollLeft(offset) {
    this.timelineNode.scrollLeft = offset;
    this._cachedTimelineScrollLeft = offset;
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
    const centerOffset = this._timelineBoundingClientRect.width / 2;
    const time =
        this.relativePositionToTime(this._timelineScrollLeft + centerOffset);
    const start = this._timeline.startTime;
    const width = this._nofChunks * kChunkWidth;
    this._lastContentWidth = parseInt(this.timelineMarkersNode.style.width);
    this._timeToPixel = width / this._timeline.duration();
    this._timeStartPixelOffset = start * this._timeToPixel;
    this.timelineChunks.style.width = `${width}px`;
    this.timelineMarkersNode.style.width = `${width}px`;
    this.timelineAnnotationsNode.style.width = `${width}px`;
    this.hitPanelNode.style.width = `${width}px`;
    this._drawMarkers();
    this._selectionHandler.update();
    this._scaleContent(width);
    this._cachedTimelineScrollLeft = this.timelineNode.scrollLeft =
        this.timeToPosition(time) - centerOffset;
  }

  _scaleContent(currentWidth) {
    if (!this._lastContentWidth) return;
    const ratio = currentWidth / this._lastContentWidth;
    this._scalableContentNode.style.transform = `scale(${ratio}, 1)`;
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
    this._drawContent();
    this._drawAnnotations(this.selectedEntry);
    this._resetCachedDimensions();
  }

  async _drawContent() {
    await delay(5);
    if (this._timeline.isEmpty()) return;
    if (this.chunks?.length != this.nofChunks) {
      this._chunks =
          this._timeline.chunks(this.nofChunks, this._legend.filterPredicate);
      console.assert(this._chunks.length == this._nofChunks);
    }
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
    this.dispatchEvent(
        new ToolTipEvent(this._focusedEntry, this.toolTipTargetNode));
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
    const style = this.toolTipTargetNode.style;
    style.left = `${chunk.index * kChunkWidth}px`;
    style.top = `${kTimelineHeight - chunk.height}px`;
    style.height = `${chunk.height}px`;
    style.width = `${kChunkVisualWidth}px`;
    return logEntry;
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
    this._timelineNode.addEventListener(
        'mousedown', e => this._handleTimeSelectionMouseDown(e));
    this._timelineNode.addEventListener(
        'mouseup', e => this._handleTimeSelectionMouseUp(e));
    this._timelineNode.addEventListener(
        'mousemove', e => this._handleTimeSelectionMouseMove(e));
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
    const selectionNode = this._selectionBackgroundNode;
    selectionNode.style.left = startPosition + 'px';
    selectionNode.style.width = delta + 'px';
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

  get _timelineNode() {
    return this._timeline.$('#timeline');
  }

  get _selectionNode() {
    return this._timeline.$('#selection');
  }

  get _selectionBackgroundNode() {
    return this._timeline.$('#selectionBackground');
  }

  get _leftHandleNode() {
    return this._timeline.$('#leftHandle');
  }

  get _rightHandleNode() {
    return this._timeline.$('#rightHandle');
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

  _handleTimeSelectionMouseDown(event) {
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

  _handleTimeSelectionMouseMove(event) {
    if (event.button !== 0) return;
    if (!this.isSelecting) return;
    const currentTime = this.positionToTime(event.clientX);
    this._timeline.dispatchEvent(new SynchronizeSelectionEvent(
        Math.min(this._selectionOriginTime, currentTime),
        Math.max(this._selectionOriginTime, currentTime)));
  }

  _handleTimeSelectionMouseUp(event) {
    if (event.button !== 0) return;
    this._selectionOriginTime = -1;
    if (this._timeSelection.start === -1) return;
    const delta = this._timeSelection.end - this._timeSelection.start;
    if (delta <= 1 || isNaN(delta)) return;
    this._timeline.dispatchEvent(new SelectTimeEvent(
        this._timeSelection.start, this._timeSelection.end));
  }
}

class Legend {
  _timeline;
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

  update() {
    const tbody = DOM.tbody();
    const missingTypes = new Set(this._typesFilters.keys());
    this._checkDurationField();
    this.selection.getBreakdown(undefined, this._enableDuration)
        .forEach(group => {
          tbody.appendChild(this._addTypeRow(group));
          missingTypes.delete(group.key);
        });
    missingTypes.forEach(key => tbody.appendChild(this._row('', key, 0, '0%')));
    if (this._timeline.selection) {
      tbody.appendChild(
          this._row('', 'Selection', this.selection.length, '100%'));
    }
    tbody.appendChild(this._row('', 'All', this._timeline.length, ''));
    this._table.tBodies[0].replaceWith(tbody);
  }

  _checkDurationField() {
    if (this._enableDuration) return;
    const example = this.selection.at(0);
    if (!example || !('duration' in example)) return;
    this._enableDuration = true;
    this._table.tHead.appendChild(DOM.td('Duration'));
    this._table.tHead.appendChild(DOM.td(''));
  }

  _row(colorNode, type, count, countPercent, duration, durationPercent) {
    const row = DOM.tr();
    row.appendChild(DOM.td(colorNode));
    const typeCell = row.appendChild(DOM.td(type));
    typeCell.setAttribute('title', type);
    row.appendChild(DOM.td(count.toString()));
    row.appendChild(DOM.td(countPercent));
    if (this._enableDuration) {
      row.appendChild(DOM.td(formatDurationMicros(duration ?? 0)));
      row.appendChild(DOM.td(durationPercent ?? '0%'));
    }
    return row
  }

  _addTypeRow(group) {
    const color = this.colorForType(group.key);
    const colorDiv = DOM.div('colorbox');
    if (this._typesFilters.get(group.key)) {
      colorDiv.style.backgroundColor = color;
    } else {
      colorDiv.style.borderColor = color;
      colorDiv.style.backgroundColor = CSSColor.backgroundImage;
    }
    let duration = 0;
    if (this._enableDuration) {
      const entries = group.entries;
      for (let i = 0; i < entries.length; i++) {
        duration += entries[i].duration;
      }
    }
    let countPercent =
        `${(group.length / this.selection.length * 100).toFixed(1)}%`;
    const row = this._row(
        colorDiv, group.key, group.length, countPercent, duration, '');
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
}
