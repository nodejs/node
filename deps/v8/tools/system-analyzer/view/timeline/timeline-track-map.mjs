// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kChunkVisualWidth, MapLogEntry} from '../../log/map.mjs';
import {CSSColor, DOM} from '../helper.mjs';

import {TimelineTrackBase} from './timeline-track-base.mjs'

DOM.defineCustomElement('view/timeline/timeline-track', 'timeline-track-map',
                        (templateText) =>
                            class TimelineTrackMap extends TimelineTrackBase {
  constructor() {
    super(templateText);
  }

  getMapStyle(map) {
    return map.edge && map.edge.from ? CSSColor.onBackgroundColor :
                                       CSSColor.onPrimaryColor;
  }

  markMap(map) {
    const [x, y] = map.position(this.chunks);
    const strokeColor = this.getMapStyle(map);
    return `<circle cx=${x} cy=${y} r=${2} stroke=${
        strokeColor} class=annotationPoint />`
  }

  markSelectedMap(map) {
    const [x, y] = map.position(this.chunks);
    const strokeColor = this.getMapStyle(map);
    return `<circle cx=${x} cy=${y} r=${3} stroke=${
        strokeColor} class=annotationPoint />`
  }

  _drawAnnotations(logEntry, time) {
    if (!(logEntry instanceof MapLogEntry)) return;
    if (!logEntry.edge) {
      this.timelineAnnotationsNode.innerHTML = '';
      return;
    }
    // Draw the trace of maps in reverse order to make sure the outgoing
    // transitions of previous maps aren't drawn over.
    const kOpaque = 1.0;
    let stack = [];
    let current = logEntry;
    while (current !== undefined) {
      stack.push(current);
      current = current.parent;
    }

    // Draw outgoing refs as fuzzy background. Skip the last map entry.
    let buffer = '';
    let nofEdges = 0;
    const kMaxOutgoingEdges = 100;
    for (let i = stack.length - 2; i >= 0; i--) {
      const map = stack[i].parent;
      nofEdges += map.children.length;
      if (nofEdges > kMaxOutgoingEdges) break;
      buffer += this.drawOutgoingEdges(map, 0.4, 1);
    }

    // Draw main connection.
    let labelOffset = 15;
    let xPrev = 0;
    for (let i = stack.length - 1; i >= 0; i--) {
      let map = stack[i];
      if (map.edge) {
        const [xTo, data] = this.drawEdge(map.edge, kOpaque, labelOffset);
        buffer += data;
        if (xTo == xPrev) {
          labelOffset += 10;
        } else {
          labelOffset = 15
        }
        xPrev = xTo;
      }
      buffer += this.markMap(map);
    }

    buffer += this.drawOutgoingEdges(logEntry, 0.9, 3);
    // Mark selected map
    buffer += this.markSelectedMap(logEntry);
    this.timelineAnnotationsNode.innerHTML = buffer;
  }

  drawEdge(edge, opacity, labelOffset = 20) {
    let buffer = '';
    if (!edge.from || !edge.to) return [-1, buffer];
    const [xFrom, yFrom] = edge.from.position(this.chunks);
    const [xTo, yTo] = edge.to.position(this.chunks);
    const sameChunk = xTo == xFrom;
    if (sameChunk) labelOffset += 10;
    const color = this._legend.colorForType(edge.type);
    const offsetX = 20;
    const midX = xFrom + (xTo - xFrom) / 2;
    const midY = (yFrom + yTo) / 2 - 100;
    if (!sameChunk) {
      if (opacity == 1.0) {
        buffer += `<path d="M ${xFrom} ${yFrom} Q ${midX} ${midY}, ${xTo} ${
            yTo}" class=strokeBG />`
      }
      buffer += `<path d="M ${xFrom} ${yFrom} Q ${midX} ${midY}, ${xTo} ${
          yTo}" stroke=${color} fill=none opacity=${opacity} />`
    } else {
      if (opacity == 1.0) {
        buffer += `<line x1=${xFrom} x2=${xTo} y1=${yFrom} y2=${
            yTo} class=strokeBG />`;
      }
      buffer += `<line x1=${xFrom} x2=${xTo} y1=${yFrom} y2=${yTo} stroke=${
          color} fill=none opacity=${opacity} />`;
    }
    if (opacity == 1.0) {
      const centerX = sameChunk ? xTo : ((xFrom / 2 + midX + xTo / 2) / 2) | 0;
      const centerY = sameChunk ? yTo : ((yFrom / 2 + midY + yTo / 2) / 2) | 0;
      const centerYTo = centerY - labelOffset;
      buffer += `<line x1=${centerX} x2=${centerX + offsetX} y1=${centerY} y2=${
          centerYTo} stroke=${color} fill=none opacity=${opacity} />`;
      buffer += `<text x=${centerX + offsetX + 2} y=${
          centerYTo} class=annotationLabel opacity=${opacity} >${
          edge.toString()}</text>`;
    }
    return [xTo, buffer];
  }

  drawOutgoingEdges(map, opacity = 1.0, max = 10, depth = 0) {
    let buffer = '';
    if (!map || depth >= max) return buffer;
    const limit = Math.min(map.children.length, 100)
    for (let i = 0; i < limit; i++) {
      const edge = map.children[i];
      const [xTo, data] = this.drawEdge(edge, opacity);
      buffer += data;
      buffer += this.drawOutgoingEdges(edge.to, opacity * 0.5, max, depth + 1);
    }
    return buffer;
  }
})