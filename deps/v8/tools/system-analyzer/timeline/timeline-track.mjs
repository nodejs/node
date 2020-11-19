// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  defineCustomElement, V8CustomElement,
  typeToColor, CSSColor
} from '../helper.mjs';
import { kChunkWidth, kChunkHeight } from "../log/map.mjs";
import {
  SelectionEvent, FocusEvent, SelectTimeEvent,
  SynchronizeSelectionEvent
} from '../events.mjs';

defineCustomElement('./timeline/timeline-track', (templateText) =>
  class TimelineTrack extends V8CustomElement {
    static SELECTION_OFFSET = 20;
    #timeline;
    #nofChunks = 400;
    #chunks;
    #selectedEntry;
    #timeToPixel;
    #timeSelection = { start: 0, end: Infinity };
    #isSelected = false;
    #timeStartOffset;
    #mouseDownTime;
    constructor() {
      super(templateText);
      this.timeline.addEventListener("scroll",
        e => this.handleTimelineScroll(e));
      this.timeline.addEventListener("mousedown",
        e => this.handleTimeSelectionMouseDown(e));
      this.timeline.addEventListener("mouseup",
        e => this.handleTimeSelectionMouseUp(e));
      this.timeline.addEventListener("mousemove",
        e => this.handleTimeSelectionMouseMove(e));
      this.backgroundCanvas = document.createElement('canvas');
      this.isLocked = false;
    }

    handleTimeSelectionMouseDown(e) {
      if (e.target.className === "chunk") return;
      this.#isSelected = true;
      this.#mouseDownTime = this.positionToTime(e.clientX);
    }
    handleTimeSelectionMouseMove(e) {
      if (!this.#isSelected) return;
      let mouseMoveTime = this.positionToTime(e.clientX);
      let startTime = this.#mouseDownTime;
      let endTime = mouseMoveTime;
      if (this.isOnLeftHandle(e.clientX)) {
        startTime = mouseMoveTime;
        endTime = this.positionToTime(this.rightHandlePosX);
      } else if (this.isOnRightHandle(e.clientX)) {
        startTime = this.positionToTime(this.leftHandlePosX);
        endTime = mouseMoveTime;
      }
      this.dispatchEvent(new SynchronizeSelectionEvent(
        Math.min(startTime, endTime),
        Math.max(startTime, endTime)));
    }
    handleTimeSelectionMouseUp(e) {
      this.#isSelected = false;
      this.dispatchEvent(new SelectTimeEvent(this.#timeSelection.start,
        this.#timeSelection.end));
    }
    isOnLeftHandle(posX) {
      return (Math.abs(this.leftHandlePosX - posX)
        <= TimelineTrack.SELECTION_OFFSET);
    }
    isOnRightHandle(posX) {
      return (Math.abs(this.rightHandlePosX - posX)
        <= TimelineTrack.SELECTION_OFFSET);
    }


    set startTime(value) {
      console.assert(
        value <= this.#timeSelection.end,
        "Selection start time greater than end time!");
      this.#timeSelection.start = value;
      this.updateSelection();
    }
    set endTime(value) {
      console.assert(
        value > this.#timeSelection.start,
        "Selection end time smaller than start time!");
      this.#timeSelection.end = value;
      this.updateSelection();
    }

    updateSelection() {
      let startTimePos = this.timeToPosition(this.#timeSelection.start);
      let endTimePos = this.timeToPosition(this.#timeSelection.end);
      this.leftHandle.style.left = startTimePos + "px";
      this.selection.style.left = startTimePos + "px";
      this.rightHandle.style.left = endTimePos + "px";
      this.selection.style.width =
        Math.abs(this.rightHandlePosX - this.leftHandlePosX) + "px";
    }

    get leftHandlePosX() {
      let leftHandlePosX = this.leftHandle.getBoundingClientRect().x;
      return leftHandlePosX;
    }
    get rightHandlePosX() {
      let rightHandlePosX = this.rightHandle.getBoundingClientRect().x;
      return rightHandlePosX;
    }

    // Maps the clicked x position to the x position on timeline canvas
    positionOnTimeline(posX) {
      let rect = this.timeline.getBoundingClientRect();
      let posClickedX = posX - rect.left + this.timeline.scrollLeft;
      return posClickedX;
    }

    positionToTime(posX) {
      let posTimelineX = this.positionOnTimeline(posX) + this.#timeStartOffset;
      return posTimelineX / this.#timeToPixel;
    }

    timeToPosition(time) {
      let posX = time * this.#timeToPixel;
      posX -= this.#timeStartOffset
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
      this.#timeline = value;
      this.updateChunks();
      this.updateTimeline();
      this.renderLegend();
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

    renderLegend() {
      let timelineLegend = this.timelineLegend;
      let timelineLegendContent = this.timelineLegendContent;
      this.removeAllChildren(timelineLegendContent);
      let row = this.tr();
      row.entries = this.data.all;
      row.classList.add('clickable');
      row.addEventListener('dblclick', e => this.handleEntryTypeDblClick(e));
      row.appendChild(this.td(""));
      let td = this.td("All");
      row.appendChild(td);
      row.appendChild(this.td(this.data.all.length));
      row.appendChild(this.td("100%"));
      timelineLegendContent.appendChild(row);
      let colorIterator = 0;
      this.#timeline.uniqueTypes.forEach((entries, type) => {
        let row = this.tr();
        row.entries = entries;
        row.classList.add('clickable');
        row.addEventListener('dblclick', e => this.handleEntryTypeDblClick(e));
        let color = typeToColor(type);
        if (color !== null) {
          let div = this.div(["colorbox"]);
          div.style.backgroundColor = color;
          row.appendChild(this.td(div));
        } else {
          row.appendChild(this.td(""));
        }
        let td = this.td(type);
        row.appendChild(td);
        row.appendChild(this.td(entries.length));
        let percent = (entries.length / this.data.all.length) * 100;
        row.appendChild(this.td(percent.toFixed(1) + "%"));
        timelineLegendContent.appendChild(row);
        colorIterator += 1;
      });
      timelineLegend.appendChild(timelineLegendContent);
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
          ctx.fillStyle = typeToColor(type);
          let height = count / total * kHeight;
          ctx.fillRect(0, y, kWidth, y + height);
          y += height;
        });
      } else {
        chunk.items.forEach(map => {
          ctx.fillStyle = typeToColor(map.type);
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
      this.#timeStartOffset = start * this.#timeToPixel;
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
        node.style.left =
          ((chunks[i].start - start) * this.#timeToPixel) + 'px';
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
      let color = typeToColor(edge.type);
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
        ctx.fillStyle = typeToColor(edge.type);
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
