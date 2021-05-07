// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kChunkHeight, kChunkWidth} from '../../log/map.mjs';
import {MapLogEntry} from '../../log/map.mjs';
import {FocusEvent, SelectionEvent, SelectTimeEvent, SynchronizeSelectionEvent, ToolTipEvent,} from '../events.mjs';
import {CSSColor, DOM, gradientStopsFromGroups, V8CustomElement} from '../helper.mjs';

DOM.defineCustomElement('view/timeline/timeline-track',
                        (templateText) =>
                            class TimelineTrack extends V8CustomElement {
  _timeline;
  _nofChunks = 400;
  _chunks;
  _selectedEntry;
  _timeToPixel;
  _timeStartOffset;
  _legend;

  _chunkMouseMoveHandler = this._handleChunkMouseMove.bind(this);
  _chunkClickHandler = this._handleChunkClick.bind(this);
  _chunkDoubleClickHandler = this._handleChunkDoubleClick.bind(this);

  constructor() {
    super(templateText);
    this._selectionHandler = new SelectionHandler(this);
    this._legend = new Legend(this.$('#legendTable'));
    this._legend.onFilter = (type) => this._handleFilterTimeline();
    this.timelineNode.addEventListener(
        'scroll', e => this._handleTimelineScroll(e));
    this.timelineNode.ondblclick = (e) =>
        this._selectionHandler.clearSelection();
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

  // Maps the clicked x position to the x position on timeline canvas
  positionOnTimeline(posX) {
    let rect = this.timelineNode.getBoundingClientRect();
    let posClickedX = posX - rect.left + this.timelineNode.scrollLeft;
    return posClickedX;
  }

  positionToTime(posX) {
    let posTimelineX = this.positionOnTimeline(posX) + this._timeStartOffset;
    return posTimelineX / this._timeToPixel;
  }

  timeToPosition(time) {
    let posX = time * this._timeToPixel;
    posX -= this._timeStartOffset;
    return posX;
  }

  get timelineCanvas() {
    return this.$('#timelineCanvas');
  }

  get timelineChunks() {
    return this.$('#timelineChunks');
  }

  get timelineNode() {
    return this.$('#timeline');
  }

  _update() {
    this._updateTimeline();
    this._legend.update();
  }

  set nofChunks(count) {
    this._nofChunks = count;
    this._updateChunks();
  }

  get nofChunks() {
    return this._nofChunks;
  }

  _updateChunks() {
    this._chunks =
        this._timeline.chunks(this.nofChunks, this._legend.filterPredicate);
    this.update();
  }

  get chunks() {
    return this._chunks;
  }

  set selectedEntry(value) {
    this._selectedEntry = value;
    if (value.edge) this.redraw();
  }

  get selectedEntry() {
    return this._selectedEntry;
  }

  set scrollLeft(offset) {
    this.timelineNode.scrollLeft = offset;
  }

  handleEntryTypeDoubleClick(e) {
    this.dispatchEvent(new SelectionEvent(e.target.parentNode.entries));
  }

  timelineIndicatorMove(offset) {
    this.timelineNode.scrollLeft += offset;
  }

  _handleTimelineScroll(e) {
    let horizontal = e.currentTarget.scrollLeft;
    this.dispatchEvent(new CustomEvent(
        'scrolltrack', {bubbles: true, composed: true, detail: horizontal}));
  }

  _createBackgroundImage(chunk) {
    const stops = gradientStopsFromGroups(
        chunk.length, chunk.height, chunk.getBreakdown(event => event.type),
        type => this._legend.colorForType(type));
    return `linear-gradient(0deg,${stops.join(',')})`;
  }

  _updateTimeline() {
    const reusableNodes = Array.from(this.timelineChunks.childNodes).reverse();
    let fragment = new DocumentFragment();
    let chunks = this.chunks;
    let max = chunks.max(each => each.size());
    let start = this._timeline.startTime;
    let end = this._timeline.endTime;
    let duration = end - start;
    this._timeToPixel = chunks.length * kChunkWidth / duration;
    this._timeStartOffset = start * this._timeToPixel;
    for (let i = 0; i < chunks.length; i++) {
      let chunk = chunks[i];
      let height = (chunk.size() / max * kChunkHeight);
      chunk.height = height;
      if (chunk.isEmpty()) continue;
      let node = reusableNodes[reusableNodes.length - 1];
      let reusedNode = false;
      if (node?.className == 'chunk') {
        reusableNodes.pop();
        reusedNode = true;
      } else {
        node = DOM.div('chunk');
        node.onmousemove = this._chunkMouseMoveHandler;
        node.onclick = this._chunkClickHandler;
        node.ondblclick = this._chunkDoubleClickHandler;
      }
      const style = node.style;
      style.left = `${((chunk.start - start) * this._timeToPixel) | 0}px`;
      style.height = `${height | 0}px`;
      style.backgroundImage = this._createBackgroundImage(chunk);
      node.chunk = chunk;
      if (!reusedNode) fragment.appendChild(node);
    }

    // Put a time marker roughly every 20 chunks.
    let expected = duration / chunks.length * 20;
    let interval = (10 ** Math.floor(Math.log10(expected)));
    let correction = Math.log10(expected / interval);
    correction = (correction < 0.33) ? 1 : (correction < 0.75) ? 2.5 : 5;
    interval *= correction;

    let time = start;
    while (time < end) {
      let timeNode = DOM.div('timestamp');
      timeNode.innerText = `${((time - start) / 1000) | 0} ms`;
      timeNode.style.left = `${((time - start) * this._timeToPixel) | 0}px`;
      fragment.appendChild(timeNode);
      time += interval;
    }

    // Remove superfluos nodes lazily, for Chrome this is a very expensive
    // operation.
    if (reusableNodes.length > 0) {
      for (const node of reusableNodes) {
        node.style.display = 'none';
      }
      setTimeout(() => {
        const range = document.createRange();
        const first = reusableNodes[reusableNodes.length - 1];
        const last = reusableNodes[0];
        range.setStartBefore(first);
        range.setEndAfter(last);
        range.deleteContents();
      }, 100);
    }
    this.timelineChunks.appendChild(fragment);
    this.redraw();
  }

  _handleChunkMouseMove(event) {
    if (this.isLocked) return false;
    if (this._selectionHandler.isSelecting) return false;
    let chunk = event.target.chunk;
    if (!chunk) return;
    if (chunk.isEmpty()) return;
    // topmost map (at chunk.height) == map #0.
    let relativeIndex = Math.round(
        event.layerY / event.target.offsetHeight * (chunk.size() - 1));
    let logEntry = chunk.at(relativeIndex);
    this.dispatchEvent(new FocusEvent(logEntry));
    this.dispatchEvent(new ToolTipEvent(logEntry.toStringLong(), event.target));
  }

  _handleChunkClick(event) {
    this.isLocked = !this.isLocked;
  }

  _handleChunkDoubleClick(event) {
    let chunk = event.target.chunk;
    if (!chunk) return;
    event.stopPropagation();
    this.dispatchEvent(new SelectTimeEvent(chunk.start, chunk.end));
  }

  redraw() {
    window.requestAnimationFrame(() => this._redraw());
  }

  _redraw() {
    if (!(this._timeline.at(0) instanceof MapLogEntry)) return;
    let canvas = this.timelineCanvas;
    let width = (this.chunks.length + 1) * kChunkWidth;
    if (width > 32767) width = 32767;
    canvas.width = width;
    canvas.height = kChunkHeight;
    let ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, kChunkHeight);
    if (!this.selectedEntry || !this.selectedEntry.edge) return;
    this.drawEdges(ctx);
  }

  setMapStyle(map, ctx) {
    ctx.fillStyle = map.edge && map.edge.from ? CSSColor.onBackgroundColor :
                                                CSSColor.onPrimaryColor;
  }

  setEdgeStyle(edge, ctx) {
    let color = this._legend.colorForType(edge.type);
    ctx.strokeStyle = color;
    ctx.fillStyle = color;
  }

  markMap(ctx, map) {
    let [x, y] = map.position(this.chunks);
    ctx.beginPath();
    this.setMapStyle(map, ctx);
    ctx.arc(x, y, 3, 0, 2 * Math.PI);
    ctx.fill();
    ctx.beginPath();
    ctx.fillStyle = CSSColor.onBackgroundColor;
    ctx.arc(x, y, 2, 0, 2 * Math.PI);
    ctx.fill();
  }

  markSelectedMap(ctx, map) {
    let [x, y] = map.position(this.chunks);
    ctx.beginPath();
    this.setMapStyle(map, ctx);
    ctx.arc(x, y, 6, 0, 2 * Math.PI);
    ctx.strokeStyle = CSSColor.onBackgroundColor;
    ctx.stroke();
  }

  drawEdges(ctx) {
    // Draw the trace of maps in reverse order to make sure the outgoing
    // transitions of previous maps aren't drawn over.
    const kMaxOutgoingEdges = 100;
    let nofEdges = 0;
    let stack = [];
    let current = this.selectedEntry;
    while (current && nofEdges < kMaxOutgoingEdges) {
      nofEdges += current.children.length;
      stack.push(current);
      current = current.parent();
    }
    ctx.save();
    this.drawOutgoingEdges(ctx, this.selectedEntry, 3);
    ctx.restore();

    let labelOffset = 15;
    let xPrev = 0;
    while (current = stack.pop()) {
      if (current.edge) {
        this.setEdgeStyle(current.edge, ctx);
        let [xTo, yTo] = this.drawEdge(ctx, current.edge, true, labelOffset);
        if (xTo == xPrev) {
          labelOffset += 8;
        } else {
          labelOffset = 15
        }
        xPrev = xTo;
      }
      this.markMap(ctx, current);
      current = current.parent();
      ctx.save();
      // this.drawOutgoingEdges(ctx, current, 1);
      ctx.restore();
    }
    // Mark selected map
    this.markSelectedMap(ctx, this.selectedEntry);
  }

  drawEdge(ctx, edge, showLabel = true, labelOffset = 20) {
    if (!edge.from || !edge.to) return [-1, -1];
    let [xFrom, yFrom] = edge.from.position(this.chunks);
    let [xTo, yTo] = edge.to.position(this.chunks);
    let sameChunk = xTo == xFrom;
    if (sameChunk) labelOffset += 8;

    ctx.beginPath();
    ctx.moveTo(xFrom, yFrom);
    let offsetX = 20;
    let offsetY = 20;
    let midX = xFrom + (xTo - xFrom) / 2;
    let midY = (yFrom + yTo) / 2 - 100;
    if (!sameChunk) {
      ctx.quadraticCurveTo(midX, midY, xTo, yTo);
    } else {
      ctx.lineTo(xTo, yTo);
    }
    if (!showLabel) {
      ctx.stroke();
    } else {
      let centerX, centerY;
      if (!sameChunk) {
        centerX = (xFrom / 2 + midX + xTo / 2) / 2;
        centerY = (yFrom / 2 + midY + yTo / 2) / 2;
      } else {
        centerX = xTo;
        centerY = yTo;
      }
      ctx.moveTo(centerX, centerY);
      ctx.lineTo(centerX + offsetX, centerY - labelOffset);
      ctx.stroke();
      ctx.textAlign = 'left';
      ctx.fillStyle = this._legend.colorForType(edge.type);
      ctx.fillText(
          edge.toString(), centerX + offsetX + 2, centerY - labelOffset);
    }
    return [xTo, yTo];
  }

  drawOutgoingEdges(ctx, map, max = 10, depth = 0) {
    if (!map) return;
    if (depth >= max) return;
    ctx.globalAlpha = 0.5 - depth * (0.3 / max);
    ctx.strokeStyle = CSSColor.timelineBackgroundColor;
    const limit = Math.min(map.children.length, 100)
    for (let i = 0; i < limit; i++) {
      let edge = map.children[i];
      this.drawEdge(ctx, edge, true);
      this.drawOutgoingEdges(ctx, edge.to, max, depth + 1);
    }
  }
});

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

  _handleTimeSelectionMouseDown(e) {
    let xPosition = e.clientX
    // Update origin time in case we click on a handle.
    if (this._isOnLeftHandle(xPosition)) {
      xPosition = this._rightHandlePosX;
    }
    else if (this._isOnRightHandle(xPosition)) {
      xPosition = this._leftHandlePosX;
    }
    this._selectionOriginTime = this.positionToTime(xPosition);
  }

  _handleTimeSelectionMouseMove(e) {
    if (!this.isSelecting) return;
    const currentTime = this.positionToTime(e.clientX);
    this._timeline.dispatchEvent(new SynchronizeSelectionEvent(
        Math.min(this._selectionOriginTime, currentTime),
        Math.max(this._selectionOriginTime, currentTime)));
  }

  _handleTimeSelectionMouseUp(e) {
    this._selectionOriginTime = -1;
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
    return this._colors.get(type);
  }

  filter(logEntry) {
    return this._typesFilters.get(logEntry.type);
  }

  update() {
    const tbody = DOM.tbody();
    const missingTypes = new Set(this._typesFilters.keys());
    this.selection.getBreakdown().forEach(group => {
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

  _row(color, type, count, percent) {
    const row = DOM.tr();
    row.appendChild(DOM.td(color));
    row.appendChild(DOM.td(type));
    row.appendChild(DOM.td(count.toString()));
    row.appendChild(DOM.td(percent));
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
    let percent = `${(group.count / this.selection.length * 100).toFixed(1)}%`;
    const row = this._row(colorDiv, group.key, group.count, percent);
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