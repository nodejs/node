// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FocusEvent, SelectionEvent, SelectTimeEvent, SynchronizeSelectionEvent} from '../events.mjs';
import {CSSColor, delay, DOM, V8CustomElement} from '../helper.mjs';
import {kChunkHeight, kChunkWidth} from '../log/map.mjs';

const kColors = [
  CSSColor.green,
  CSSColor.violet,
  CSSColor.orange,
  CSSColor.yellow,
  CSSColor.primaryColor,
  CSSColor.red,
  CSSColor.blue,
  CSSColor.yellow,
  CSSColor.secondaryColor,
];

DOM.defineCustomElement('./timeline/timeline-track',
                        (templateText) =>
                            class TimelineTrack extends V8CustomElement {
  // TODO turn into static field once Safari supports it.
  static get SELECTION_OFFSET() {
    return 10
  };
  _timeline;
  _nofChunks = 400;
  _chunks;
  _selectedEntry;
  _timeToPixel;
  _timeSelection = {start: -1, end: Infinity};
  _timeStartOffset;
  _selectionOriginTime;
  _typeToColor;
  constructor() {
    super(templateText);
    this.timeline.addEventListener('scroll', e => this.handleTimelineScroll(e));
    this.timeline.addEventListener(
        'mousedown', e => this.handleTimeSelectionMouseDown(e));
    this.timeline.addEventListener(
        'mouseup', e => this.handleTimeSelectionMouseUp(e));
    this.timeline.addEventListener(
        'mousemove', e => this.handleTimeSelectionMouseMove(e));
    this.backgroundCanvas = document.createElement('canvas');
    this.isLocked = false;
  }

  handleTimeSelectionMouseDown(e) {
    let xPosition = e.clientX
    // Update origin time in case we click on a handle.
    if (this.isOnLeftHandle(xPosition)) {
      xPosition = this.rightHandlePosX;
    }
    else if (this.isOnRightHandle(xPosition)) {
      xPosition = this.leftHandlePosX;
    }
    this._selectionOriginTime = this.positionToTime(xPosition);
  }

  isOnLeftHandle(posX) {
    return (
        Math.abs(this.leftHandlePosX - posX) <= TimelineTrack.SELECTION_OFFSET);
  }

  isOnRightHandle(posX) {
    return (
        Math.abs(this.rightHandlePosX - posX) <=
        TimelineTrack.SELECTION_OFFSET);
  }

  handleTimeSelectionMouseMove(e) {
    if (!this._isSelecting) return;
    const currentTime = this.positionToTime(e.clientX);
    this.dispatchEvent(new SynchronizeSelectionEvent(
        Math.min(this._selectionOriginTime, currentTime),
        Math.max(this._selectionOriginTime, currentTime)));
  }

  handleTimeSelectionMouseUp(e) {
    this._selectionOriginTime = -1;
    const delta = this._timeSelection.end - this._timeSelection.start;
    if (delta <= 1 || isNaN(delta)) return;
    this.dispatchEvent(new SelectTimeEvent(
        this._timeSelection.start, this._timeSelection.end));
  }

  set timeSelection(selection) {
    this._timeSelection.start = selection.start;
    this._timeSelection.end = selection.end;
    this.updateSelection();
  }

  get _isSelecting() {
    return this._selectionOriginTime >= 0;
  }

  updateSelection() {
    const startPosition = this.timeToPosition(this._timeSelection.start);
    const endPosition = this.timeToPosition(this._timeSelection.end);
    const delta = endPosition - startPosition;
    this.leftHandle.style.left = startPosition + 'px';
    this.selection.style.left = startPosition + 'px';
    this.rightHandle.style.left = endPosition + 'px';
    this.selection.style.width = delta + 'px';
  }

  get leftHandlePosX() {
    return this.leftHandle.getBoundingClientRect().x;
  }

  get rightHandlePosX() {
    return this.rightHandle.getBoundingClientRect().x;
  }

  // Maps the clicked x position to the x position on timeline canvas
  positionOnTimeline(posX) {
    let rect = this.timeline.getBoundingClientRect();
    let posClickedX = posX - rect.left + this.timeline.scrollLeft;
    return posClickedX;
  }

  positionToTime(posX) {
    let posTimelineX = this.positionOnTimeline(posX) + this._timeStartOffset;
    return posTimelineX / this._timeToPixel;
  }

  timeToPosition(time) {
    let posX = time * this._timeToPixel;
    posX -= this._timeStartOffset
    return posX;
  }

  get leftHandle() {
    return this.$('.leftHandle');
  }

  get rightHandle() {
    return this.$('.rightHandle');
  }

  get selection() {
    return this.$('.selection');
  }

  get timelineCanvas() {
    return this.$('#timelineCanvas');
  }

  get timelineChunks() {
    return this.$('#timelineChunks');
  }

  get timeline() {
    return this.$('#timeline');
  }

  get timelineLegend() {
    return this.$('#legend');
  }

  get timelineLegendContent() {
    return this.$('#legendContent');
  }

  set data(value) {
    this._timeline = value;
    this._resetTypeToColorCache();
    this.update();
  }

  _update() {
    this._updateChunks();
    this._updateTimeline();
    this._renderLegend();
  }

  _resetTypeToColorCache() {
    this._typeToColor = new Map();
    let lastIndex = 0;
    for (const type of this.data.uniqueTypes.keys()) {
      this._typeToColor.set(type, kColors[lastIndex++]);
    }
  }

  get data() {
    return this._timeline;
  }

  set nofChunks(count) {
    this._nofChunks = count;
    this.update();
  }

  get nofChunks() {
    return this._nofChunks;
  }

  _updateChunks() {
    this._chunks = this.data.chunks(this.nofChunks);
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
    this.timeline.scrollLeft = offset;
  }

  typeToColor(type) {
    return this._typeToColor.get(type);
  }

  _renderLegend() {
    let timelineLegendContent = this.timelineLegendContent;
    DOM.removeAllChildren(timelineLegendContent);
    this._timeline.uniqueTypes.forEach((entries, type) => {
      let row = DOM.tr('clickable');
      row.entries = entries;
      row.addEventListener('dblclick', e => this.handleEntryTypeDblClick(e));
      let color = this.typeToColor(type);
      if (color !== null) {
        let div = DOM.div('colorbox');
        div.style.backgroundColor = color;
        row.appendChild(DOM.td(div));
      } else {
        row.appendChild(DOM.td());
      }
      let td = DOM.td(type);
      row.appendChild(td);
      row.appendChild(DOM.td(entries.length));
      let percent = (entries.length / this.data.all.length) * 100;
      row.appendChild(DOM.td(percent.toFixed(1) + '%'));
      timelineLegendContent.appendChild(row);
    });
    // Add Total row.
    let row = DOM.tr();
    row.appendChild(DOM.td(''));
    row.appendChild(DOM.td('All'));
    row.appendChild(DOM.td(this.data.all.length));
    row.appendChild(DOM.td('100%'));
    timelineLegendContent.appendChild(row);
    this.timelineLegend.appendChild(timelineLegendContent);
  }

  handleEntryTypeDblClick(e) {
    this.dispatchEvent(new SelectionEvent(e.target.parentNode.entries));
  }

  timelineIndicatorMove(offset) {
    this.timeline.scrollLeft += offset;
  }

  handleTimelineScroll(e) {
    let horizontal = e.currentTarget.scrollLeft;
    this.dispatchEvent(new CustomEvent(
        'scrolltrack', {bubbles: true, composed: true, detail: horizontal}));
  }

  async setChunkBackgrounds(backgroundTodo) {
    const kMaxDuration = 50;
    let lastTime = 0;
    for (let [chunk, node] of backgroundTodo) {
      const current = performance.now();
      if (current - lastTime > kMaxDuration) {
        await delay(25);
        lastTime = current;
      }
      this.setChunkBackground(chunk, node);
    }
  }

  setChunkBackground(chunk, node) {
    // Render the types of transitions as bar charts
    const kHeight = chunk.height;
    const kWidth = 1;
    this.backgroundCanvas.width = kWidth;
    this.backgroundCanvas.height = kHeight;
    let ctx = this.backgroundCanvas.getContext('2d');
    ctx.clearRect(0, 0, kWidth, kHeight);
    let y = 0;
    let total = chunk.size();
    let type, count;
    if (true) {
      chunk.getBreakdown(map => map.type).forEach(([type, count]) => {
        ctx.fillStyle = this.typeToColor(type);
        let height = count / total * kHeight;
        ctx.fillRect(0, y, kWidth, y + height);
        y += height;
      });
    } else {
      chunk.items.forEach(map => {
        ctx.fillStyle = this.typeToColor(map.type);
        let y = chunk.yOffset(map);
        ctx.fillRect(0, y, kWidth, y + 1);
      });
    }

    let imageData = this.backgroundCanvas.toDataURL('image/webp', 0.2);
    node.style.backgroundImage = `url(${imageData})`;
  }

  _updateTimeline() {
    let chunksNode = this.timelineChunks;
    DOM.removeAllChildren(chunksNode);
    let chunks = this.chunks;
    let max = chunks.max(each => each.size());
    let start = this.data.startTime;
    let end = this.data.endTime;
    let duration = end - start;
    this._timeToPixel = chunks.length * kChunkWidth / duration;
    this._timeStartOffset = start * this._timeToPixel;
    let addTimestamp = (time, name) => {
      let timeNode = DOM.div('timestamp');
      timeNode.innerText = name;
      timeNode.style.left = ((time - start) * this._timeToPixel) + 'px';
      chunksNode.appendChild(timeNode);
    };
    let backgroundTodo = [];
    for (let i = 0; i < chunks.length; i++) {
      let chunk = chunks[i];
      let height = (chunk.size() / max * kChunkHeight);
      chunk.height = height;
      if (chunk.isEmpty()) continue;
      let node = DOM.div();
      node.className = 'chunk';
      node.style.left = ((chunks[i].start - start) * this._timeToPixel) + 'px';
      node.style.height = height + 'px';
      node.chunk = chunk;
      node.addEventListener('mousemove', e => this.handleChunkMouseMove(e));
      node.addEventListener('click', e => this.handleChunkClick(e));
      node.addEventListener('dblclick', e => this.handleChunkDoubleClick(e));
      backgroundTodo.push([chunk, node])
      chunksNode.appendChild(node);
    }
    this.setChunkBackgrounds(backgroundTodo);

    // Put a time marker roughly every 20 chunks.
    let expected = duration / chunks.length * 20;
    let interval = (10 ** Math.floor(Math.log10(expected)));
    let correction = Math.log10(expected / interval);
    correction = (correction < 0.33) ? 1 : (correction < 0.75) ? 2.5 : 5;
    interval *= correction;

    let time = start;
    while (time < end) {
      addTimestamp(time, ((time - start) / 1000) + ' ms');
      time += interval;
    }
    this.redraw();
  }

  handleChunkMouseMove(event) {
    if (this.isLocked) return false;
    if (this._isSelecting) return false;
    let chunk = event.target.chunk;
    if (!chunk) return;
    // topmost map (at chunk.height) == map #0.
    let relativeIndex =
        Math.round(event.layerY / event.target.offsetHeight * chunk.size());
    let map = chunk.at(relativeIndex);
    this.dispatchEvent(new FocusEvent(map));
  }

  handleChunkClick(event) {
    this.isLocked = !this.isLocked;
  }

  handleChunkDoubleClick(event) {
    let chunk = event.target.chunk;
    if (!chunk) return;
    this.dispatchEvent(new SelectTimeEvent(chunk.start, chunk.end));
  }

  redraw() {
    let canvas = this.timelineCanvas;
    canvas.width = (this.chunks.length + 1) * kChunkWidth;
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
    let color = this.typeToColor(edge.type);
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
      ctx.fillStyle = this.typeToColor(edge.type);
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
