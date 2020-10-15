// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  defineCustomElement, V8CustomElement,
  transitionTypeToColor, CSSColor
} from '../helper.mjs';
import { kChunkWidth, kChunkHeight } from '../map-processor.mjs';
import { SelectionEvent, FocusEvent, SelectTimeEvent } from '../events.mjs';

defineCustomElement('./timeline/timeline-track', (templateText) =>
  class TimelineTrack extends V8CustomElement {
    #timeline;
    #nofChunks = 400;
    #chunks;
    #selectedEntry;
    #timeToPixel;
    #timeSelection = { start: 0, end: Infinity };
    constructor() {
      super(templateText);
      this.timeline.addEventListener("mousedown",
        e => this.handleTimeRangeSelectionStart(e));
      this.timeline.addEventListener("mouseup",
        e => this.handleTimeRangeSelectionEnd(e));
      this.timeline.addEventListener("scroll",
        e => this.handleTimelineScroll(e));
      this.backgroundCanvas = document.createElement('canvas');
      this.isLocked = false;
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

    get timelineLegendContent() {
      return this.$('#timelineLegendContent');
    }

    set data(value) {
      this.#timeline = value;
      this.updateChunks();
      this.updateTimeline();
      this.updateStats();
    }

    get data() {
      return this.#timeline;
    }

    set nofChunks(count) {
      this.#nofChunks = count;
      this.updateChunks();
      this.updateTimeline();
    }
    get nofChunks() {
      return this.#nofChunks;
    }
    updateChunks() {
      this.#chunks = this.data.chunks(this.nofChunks);
    }
    get chunks() {
      return this.#chunks;
    }
    set selectedEntry(value) {
      this.#selectedEntry = value;
      if (value.edge) this.redraw();
    }
    get selectedEntry() {
      return this.#selectedEntry;
    }

    set scrollLeft(offset) {
      this.timeline.scrollLeft = offset;
    }

    updateStats() {
      let unique = new Map();
      for (const entry of this.data.all) {
        if (!unique.has(entry.type)) {
          unique.set(entry.type, [entry]);
        } else {
          unique.get(entry.type).push(entry);
        }
      }
      this.renderStatsWindow(unique);
    }

    renderStatsWindow(unique) {
      let timelineLegendContent = this.timelineLegendContent;
      this.removeAllChildren(timelineLegendContent);
      let fragment = document.createDocumentFragment();
      let colorIterator = 0;
      unique.forEach((entries, type) => {
        let dt = document.createElement("dt");
        dt.innerHTML = entries.length;
        dt.style.backgroundColor = transitionTypeToColor(type);
        dt.style.color = CSSColor.surfaceColor;
        fragment.appendChild(dt);
        let dd = document.createElement("dd");
        dd.innerHTML = type;
        dd.entries = entries;
        dd.addEventListener('dblclick', e => this.handleEntryTypeDblClick(e));
        fragment.appendChild(dd);
        colorIterator += 1;
      });
      timelineLegendContent.appendChild(fragment);
    }

    handleEntryTypeDblClick(e) {
      this.dispatchEvent(new SelectionEvent(e.target.entries));
    }

    timelineIndicatorMove(offset) {
      this.timeline.scrollLeft += offset;
    }

    handleTimeRangeSelectionStart(e) {
      this.#timeSelection.start = this.positionToTime(e.clientX);
    }

    handleTimeRangeSelectionEnd(e) {
      this.#timeSelection.end = this.positionToTime(e.clientX);
      this.dispatchEvent(new SelectTimeEvent(
        Math.min(this.#timeSelection.start, this.#timeSelection.end),
        Math.max(this.#timeSelection.start, this.#timeSelection.end)));
    }

    positionToTime(posX) {
      let rect = this.timeline.getBoundingClientRect();
      let posClickedX = posX - rect.left + this.timeline.scrollLeft;
      let selectedTime = posClickedX / this.#timeToPixel;
      return selectedTime;
    }

    handleTimelineScroll(e) {
      let horizontal = e.currentTarget.scrollLeft;
      this.dispatchEvent(new CustomEvent(
        'scrolltrack', {
        bubbles: true, composed: true,
        detail: horizontal
      }));
    }

    asyncSetTimelineChunkBackground(backgroundTodo) {
      const kIncrement = 100;
      let start = 0;
      let delay = 1;
      while (start < backgroundTodo.length) {
        let end = Math.min(start + kIncrement, backgroundTodo.length);
        setTimeout((from, to) => {
          for (let i = from; i < to; i++) {
            let [chunk, node] = backgroundTodo[i];
            this.setTimelineChunkBackground(chunk, node);
          }
        }, delay++, start, end);
        start = end;
      }
    }

    setTimelineChunkBackground(chunk, node) {
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
          ctx.fillStyle = transitionTypeToColor(type);
          let height = count / total * kHeight;
          ctx.fillRect(0, y, kWidth, y + height);
          y += height;
        });
      } else {
        chunk.items.forEach(map => {
          ctx.fillStyle = transitionTypeToColor(map.type);
          let y = chunk.yOffset(map);
          ctx.fillRect(0, y, kWidth, y + 1);
        });
      }

      let imageData = this.backgroundCanvas.toDataURL('image/webp', 0.2);
      node.style.backgroundImage = 'url(' + imageData + ')';
    }

    updateTimeline() {
      let chunksNode = this.timelineChunks;
      this.removeAllChildren(chunksNode);
      let chunks = this.chunks;
      let max = chunks.max(each => each.size());
      let start = this.data.startTime;
      let end = this.data.endTime;
      let duration = end - start;
      this.#timeToPixel = chunks.length * kChunkWidth / duration;
      let addTimestamp = (time, name) => {
        let timeNode = this.div('timestamp');
        timeNode.innerText = name;
        timeNode.style.left = ((time - start) * this.#timeToPixel) + 'px';
        chunksNode.appendChild(timeNode);
      };
      let backgroundTodo = [];
      for (let i = 0; i < chunks.length; i++) {
        let chunk = chunks[i];
        let height = (chunk.size() / max * kChunkHeight);
        chunk.height = height;
        if (chunk.isEmpty()) continue;
        let node = this.div();
        node.className = 'chunk';
        node.style.left = (chunks[i].start * this.#timeToPixel) + 'px';
        node.style.height = height + 'px';
        node.chunk = chunk;
        node.addEventListener('mousemove', e => this.handleChunkMouseMove(e));
        node.addEventListener('click', e => this.handleChunkClick(e));
        node.addEventListener('dblclick', e => this.handleChunkDoubleClick(e));
        backgroundTodo.push([chunk, node])
        chunksNode.appendChild(node);
      }
      this.asyncSetTimelineChunkBackground(backgroundTodo)

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
      this.drawOverview();
      this.redraw();
    }

    handleChunkMouseMove(event) {
      if (this.isLocked) return false;
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
      this.isLocked = true;
      let chunk = event.target.chunk;
      if (!chunk) return;
      let maps = chunk.items;
      this.dispatchEvent(new SelectionEvent(maps));
    }

    drawOverview() {
      const height = 50;
      const kFactor = 2;
      let canvas = this.backgroundCanvas;
      canvas.height = height;
      canvas.width = window.innerWidth;
      let ctx = canvas.getContext('2d');
      let chunks = this.data.chunkSizes(canvas.width * kFactor);
      let max = chunks.max();
      ctx.clearRect(0, 0, canvas.width, height);
      ctx.fillStyle = CSSColor.onBackgroundColor;
      ctx.beginPath();
      ctx.moveTo(0, height);
      for (let i = 0; i < chunks.length; i++) {
        ctx.lineTo(i / kFactor, height - chunks[i] / max * height);
      }
      ctx.lineTo(chunks.length, height);
      ctx.strokeStyle = CSSColor.onBackgroundColor;
      ctx.stroke();
      ctx.closePath();
      ctx.fill();
      let imageData = canvas.toDataURL('image/webp', 0.2);
      this.dispatchEvent(new CustomEvent(
        'overviewupdate', {
        bubbles: true, composed: true,
        detail: imageData
      }));
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
      ctx.fillStyle = map.edge && map.edge.from ?
        CSSColor.onBackgroundColor : CSSColor.onPrimaryColor;
    }

    setEdgeStyle(edge, ctx) {
      let color = transitionTypeToColor(edge.type);
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
        ctx.strokeStyle = CSSColor.onBackgroundColor;
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
        ctx.strokeStyle = CSSColor.onBackgroundColor;
        ctx.moveTo(centerX, centerY);
        ctx.lineTo(centerX + offsetX, centerY - labelOffset);
        ctx.stroke();
        ctx.textAlign = 'left';
        ctx.fillStyle = CSSColor.onBackgroundColor;
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
  }
);
