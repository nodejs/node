// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Timeline} from '../../timeline.mjs';
import {SelectTimeEvent} from '../events.mjs';
import {CSSColor, delay, SVG} from '../helper.mjs';

import {TimelineTrackBase} from './timeline-track-base.mjs'

const kItemHeight = 8;

export class TimelineTrackStackedBase extends TimelineTrackBase {
  _originalContentWidth = 0;
  _drawableItems = new Timeline();

  _updateChunks() {
    // We don't need to update the chunks here.
    this._updateDimensions();
    this.requestUpdate();
  }

  set data(timeline) {
    super.data = timeline;
    this._contentWidth = 0;
    if (timeline.values.length > 0) this._prepareDrawableItems();
  }

  _handleDoubleClick(event) {
    if (event.button !== 0) return;
    this._selectionHandler.clearSelection();
    const item = this._getDrawableItemForEvent(event);
    if (item === undefined) return;
    event.stopImmediatePropagation();
    this.dispatchEvent(new SelectTimeEvent(item.startTime, item.endTime));
    return false;
  }

  _getStackDepthForEvent(event) {
    return Math.floor(event.layerY / kItemHeight) - 1;
  }

  _getDrawableItemForEvent(event) {
    const depth = this._getStackDepthForEvent(event);
    const time = this.positionToTime(event.pageX);
    const index = this._drawableItems.find(time);
    for (let i = index - 1; i > 0; i--) {
      const item = this._drawableItems.at(i);
      if (item.depth != depth) continue;
      if (item.endTime < time) continue;
      return item;
    }
    return undefined;
  }

  _drawableItemToLogEntry(item) {
    return item;
  }

  _getEntryForEvent(event) {
    const item = this._getDrawableItemForEvent(event);
    const logEntry = this._drawableItemToLogEntry(item);
    if (item === undefined) return undefined;
    const node = this.getToolTipTargetNode(logEntry);
    if (!node) return logEntry;
    const style = node.style;
    style.left = `${event.layerX}px`;
    style.top = `${(item.depth + 1) * kItemHeight}px`;
    style.height = `${kItemHeight}px`
    return logEntry;
  }

  _prepareDrawableItems() {
    // Subclass responsibility.
  }

  _adjustStackDepth(maxDepth) {
    // Account for empty top line
    maxDepth++;
    this._adjustHeight(maxDepth * kItemHeight);
  }

  _scaleContent(currentWidth) {
    if (this._originalContentWidth == 0) return;
    // Instead of repainting just scale the content.
    const ratio = currentWidth / this._originalContentWidth;
    this._scalableContentNode.style.transform = `scale(${ratio}, 1)`;
    this.style.setProperty('--txt-scale', `scale(${1 / ratio}, 1)`);
    return ratio;
  }

  async _drawContent() {
    if (this._originalContentWidth > 0) return;
    this._originalContentWidth = parseInt(this.timelineMarkersNode.style.width);
    this._scalableContentNode.innerHTML = '';
    let buffer = '';
    const add = async () => {
      const svg = SVG.svg();
      svg.innerHTML = buffer;
      this._scalableContentNode.appendChild(svg);
      buffer = '';
      await delay(50);
    };
    const items = this._drawableItems.values;
    for (let i = 0; i < items.length; i++) {
      if ((i % 3000) == 0) await add();
      buffer += this._drawItem(items[i], i);
    }
    add();
  }

  _drawItem(item, i, outline = false) {
    const x = roundTo3Digits(this.timeToPosition(item.time));
    const y = (item.depth + 1) * kItemHeight;
    let width = roundTo3Digits(item.duration * this._timeToPixel);
    if (outline) {
      return `<rect x=${x} y=${y} width=${width} height=${
          kItemHeight - 1} class=fs />`;
    }
    let color = this._legend.colorForType(item.type);
    if (i % 2 == 1) {
      color = CSSColor.darken(color, 20);
    }
    return `<rect x=${x} y=${y} width=${width} height=${kItemHeight - 1} fill=${
        color} class=f />`;
  }

  _drawItemText(item) {
    const type = item.type;
    const kHeight = 9;
    const x = this.timeToPosition(item.time);
    const y = item.depth * (kHeight + 1);
    let width = item.duration * this._timeToPixel;
    width -= width * 0.1;

    let buffer = '';
    if (width < 15 || type == 'Other') return buffer;
    const rawName = item.entry.getName();
    if (rawName.length == 0) return buffer;
    const kChartWidth = 5;
    const maxChars = Math.floor(width / kChartWidth)
    const text = rawName.substr(0, maxChars);
    buffer += `<text x=${x + 1} y=${y - 3} class=txt>${text}</text>`
    return buffer;
  }
}

function roundTo3Digits(value) {
  return ((value * 1000) | 0) / 1000;
}
