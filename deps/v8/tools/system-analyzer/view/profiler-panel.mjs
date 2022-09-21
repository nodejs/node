// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CodeEntry} from '../../codemap.mjs';
import {delay, simpleHtmlEscape} from '../helper.mjs';
import {DeoptLogEntry} from '../log/code.mjs';
import {TickLogEntry} from '../log/tick.mjs';
import {Flame, FlameBuilder, ProfileNode} from '../profiling.mjs';
import {Timeline} from '../timeline.mjs';

import {FocusEvent, SelectRelatedEvent, ToolTipEvent} from './events.mjs';
import {CollapsableElement, CSSColor, DOM, LazyTable} from './helper.mjs';
import {Track} from './timeline/timeline-overview.mjs';

DOM.defineCustomElement('view/profiler-panel',
                        (templateText) =>
                            class ProfilerPanel extends CollapsableElement {
  /** @type {Timeline<TickLogEntry>} */
  _timeline;
  /** @type {Timeline<TickLogEntry> | TickLogEntry[]} */
  _displayedLogEntries;
  /** @type {Timeline<TickLogEntry> | TickLogEntry[]} */
  _selectedLogEntries;
  /** @type {ProfileNode[]} */
  _profileNodes = [];
  /** @type {Map<CodeEntry, ProfileNode>} */
  _profileNodeMap;

  constructor() {
    super(templateText);
    this._tableNode = this.$('#table');
    this._tableNode.onclick = this._handleRowClick.bind(this);
    this._showAllRadio = this.$('#show-all');
    this._showAllRadio.onclick = _ => this._showEntries(this._timeline);
    this._showTimeRangeRadio = this.$('#show-timerange');
    this._showTimeRangeRadio.onclick = _ =>
        this._showEntries(this._timeline.selectionOrSelf);
    this._showSelectionRadio = this.$('#show-selection');
    this._showSelectionRadio.onclick = _ =>
        this._showEntries(this._selectedLogEntries);
    /** @type {TimelineOverview<TickLogEntry>} */
    this._timelineOverview = this.$('#overview');
    this._timelineOverview.countCallback = (tick, /* trick,*/ track) => {
      let count = 0;
      for (let j = 0; j < tick.stack.length; j++) {
        if (track.hasEntry(tick.stack[j])) count++;
      }
      return count;
    };
    this._flameChart = this.$('#flameChart');
    this._flameChart.onmousemove = this._handleFlameChartMouseMove.bind(this);
    this._flameChart.onclick = this._handleFlameChartClick.bind(this);
  }

  /** @param {Timeline<TickLogEntry>} timeline */
  set timeline(timeline) {
    this._timeline = timeline;
    this._timelineOverview.timeline = timeline;
  }

  /** @param {Timeline<TickLogEntry> | TickLogEntry[]} entries */
  set selectedLogEntries(entries) {
    if (entries === this._timeline) {
      this._showAllRadio.click();
    } else if (entries === this._timeline.selection) {
      this._showTimeRangeRadio.click();
    } else {
      this._selectedLogEntries = entries;
      this._showSelectionRadio.click();
    }
  }

  /** @param {Timeline<TickLogEntry> | TickLogEntry[]} entries */
  _showEntries(entries) {
    this._displayedLogEntries = entries;
    this.requestUpdate();
  }

  _update() {
    this._profileNodeMap = new Map();
    const entries = this._displayedLogEntries ?
        (this._displayedLogEntries.values ?? []) :
        (this._timeline?.values ?? []);
    let totalDuration = 0;
    let totalEntries = 0;
    for (let i = 0; i < entries.length; i++) {
      /** @type {TickLogEntry} */
      const tick = entries[i];
      totalDuration += tick.duration;
      const stack = tick.stack;
      let prevCodeEntry;
      let prevStatsEntry;
      for (let j = 0; j < stack.length; j++) {
        const codeEntry = stack[j];
        totalEntries++;
        let statsEntry = this._profileNodeMap.get(codeEntry);
        if (statsEntry === undefined) {
          statsEntry = new ProfileNode(codeEntry);
          this._profileNodeMap.set(codeEntry, statsEntry);
        }
        statsEntry.ticksAndPosition.push(tick, j);
        if (prevCodeEntry !== undefined) {
          statsEntry.inCodeEntries.push(prevCodeEntry);
          prevStatsEntry.outCodeEntries.push(codeEntry);
        }
        prevCodeEntry = codeEntry;
        prevStatsEntry = statsEntry;
      }
    }

    this._profileNodes = Array.from(this._profileNodeMap.values());
    this._profileNodes.sort((a, b) => b.selfCount() - a.selfCount());

    const body = DOM.tbody();
    let buffer = [];
    for (let id = 0; id < this._profileNodes.length; id++) {
      /** @type {ProfileNode} */
      const node = this._profileNodes[id];
      /** @type {CodeEntry} */
      const codeEntry = node.codeEntry;
      buffer.push(`<tr data-id=${id} class=clickable >`);
      buffer.push(`<td class=r >${node.selfCount()}</td>`);
      const selfPercent = (node.selfCount() / entries.length * 100).toFixed(1);
      buffer.push(`<td class=r >${selfPercent}%</td>`);
      buffer.push(`<td class=r >${node.totalCount()}</td>`);
      const totalPercent = (node.totalCount() / totalEntries * 100).toFixed(1);
      buffer.push(`<td class=r >${totalPercent}%</td>`);
      if (node.isLeaf()) {
        buffer.push('<td></td>');
      } else {
        buffer.push('<td class=aC >▸</td>');
      }
      if (typeof codeEntry === 'number') {
        buffer.push('<td></td>');
        buffer.push(`<td>${codeEntry}</td>`);
        buffer.push('<td></td>');
      } else {
        const logEntry = codeEntry.logEntry;
        let sourcePositionString = logEntry.sourcePosition?.toString() ?? '';
        if (logEntry.type == 'SHARED_LIB') {
          sourcePositionString = logEntry.name;
        }
        buffer.push(`<td>${logEntry.type}</td>`);
        buffer.push(
            `<td class=nm >${simpleHtmlEscape(logEntry.shortName)}</td>`);
        buffer.push(
            `<td class=sp >${simpleHtmlEscape(sourcePositionString)}</td>`);
      }
      buffer.push('</tr>');
    }
    body.innerHTML = buffer.join('');
    this._tableNode.replaceChild(body, this._tableNode.tBodies[0]);

    this._updateOverview(this._profileNodes[0])
  }

  _handleRowClick(e) {
    let node = e.target;
    let dataId = null;
    try {
      while (dataId === null) {
        dataId = node.getAttribute('data-id');
        node = node.parentNode;
        if (!node) return;
      }
    } catch (e) {
      // getAttribute can throw, this is the lazy way out if we click on the
      // title (or anywhere that doesn't have a data-it on any parent).
      return;
    }
    const profileNode = this._profileNodes[dataId];
    const className = e.target.className;
    if (className == 'aC') {
      e.target.className = 'aO';
      return;
    } else if (className == 'aO') {
      e.target.className = 'aC';
      return;
    } else if (className == 'sp' || className == 'nm') {
      // open source position
      const codeEntry = profileNode?.codeEntry;
      if (codeEntry) {
        if (e.shiftKey) {
          this.dispatchEvent(new SelectRelatedEvent(codeEntry));
          return;
        } else if (codeEntry.sourcePosition) {
          this.dispatchEvent(new FocusEvent(codeEntry.sourcePosition));
          return;
        }
      }
    }
    // Default operation: show overview
    this._updateOverview(profileNode);
    this._updateFlameChart(profileNode);
  }

  _updateOverview(profileNode) {
    if (profileNode === undefined) {
      this._timelineOverview.tracks = [];
      return;
    }
    const mainCode = profileNode.codeEntry;
    const secondaryCodeEntries = [];
    const deopts = [];
    const codeCreation = typeof mainCode == 'number' ? [] : [mainCode.logEntry];
    if (mainCode.func?.codeEntries.size > 1) {
      for (let dynamicCode of mainCode.func.codeEntries) {
        for (let related of dynamicCode.logEntry.relatedEntries()) {
          if (related instanceof DeoptLogEntry) deopts.push(related);
        }
        if (dynamicCode === profileNode.codeEntry) continue;
        codeCreation.push(dynamicCode.logEntry);
        secondaryCodeEntries.push(dynamicCode);
      }
    }
    this._timelineOverview.tracks = [
      Track.continuous([mainCode], CSSColor.primaryColor),
      Track.continuous(secondaryCodeEntries, CSSColor.secondaryColor),
      Track.discrete(deopts, CSSColor.red),
      Track.discrete(codeCreation, CSSColor.green),
    ];
  }

  async _updateFlameChart(profileNode) {
    await delay(100);
    const codeEntry = profileNode.codeEntry;
    const stacksIn = profileNode.stacksIn();
    // Reverse the stack so the FlameBuilder groups the top-most frame
    for (let i = 0; i < stacksIn.length; i++) {
      stacksIn[i].reverse();
    }
    const stacksOut = profileNode.stacksOut();

    const flameBuilderIn = FlameBuilder.forTicks(stacksIn);
    const flameBuilderOut = FlameBuilder.forTicks(stacksOut);

    let fragment = new DocumentFragment();
    const kItemHeight = 12;
    // One empty line at the beginning
    const maxInDepth = Math.max(2, flameBuilderIn.maxDepth + 1);
    let centerDepth = maxInDepth;
    for (const flame of flameBuilderIn.flames) {
      // Ignore padded frames.
      if (flame.logEntry === undefined) continue;
      const codeEntry = flame.logEntry.entry;
      const flameProfileNode = this._profileNodeMap.get(codeEntry);
      const y = (centerDepth - flame.depth - 1) * kItemHeight;
      fragment.appendChild(
          this._createFlame(flame, flameProfileNode, y, 'fsIn'));
    }

    // Add spacing:
    centerDepth++;
    const y = centerDepth * kItemHeight;
    // Create fake Flame for the main entry;
    const centerFlame =
        new Flame(0, codeEntry.logEntry, 0, profileNode.totalCount());
    fragment.appendChild(
        this._createFlame(centerFlame, profileNode, y, 'fsMain'));

    // Add spacing:
    centerDepth += 2;

    for (const flame of flameBuilderOut.flames) {
      if (flame.logEntry === undefined) continue;
      const codeEntry = flame.logEntry.entry;
      const flameProfileNode = this._profileNodeMap.get(codeEntry);
      const y = (flame.depth + centerDepth) * kItemHeight;
      fragment.appendChild(
          this._createFlame(flame, flameProfileNode, y, 'fsOut'));
    }
    this.$('#flameChartFlames').replaceChildren(fragment);

    this.$('#flameChartIn').style.height = (maxInDepth * kItemHeight) + 'px';
    this.$('#flameChartSelected').style.top =
        ((maxInDepth + 1) * kItemHeight) + 'px';
    this.$('#flameChartOut').style.top = (centerDepth * kItemHeight) + 'px';
    this.$('#flameChartOut').style.height =
        (flameBuilderOut.maxDepth * kItemHeight) + 'px';
  }

  _createFlame(flame, profileNode, y, className) {
    const ticksToPixel = 4;
    const x = flame.time * ticksToPixel;
    const width = flame.duration * ticksToPixel;
    const div = DOM.div(className);
    div.style = `left:${x}px;top:${y}px;width:${width}px`;
    div.innerText = flame.name;
    div.data = profileNode;
    return div;
  }

  _handleFlameChartMouseMove(e) {
    const profileNode = e.target.data;
    if (!profileNode) return;
    const logEntry = profileNode.codeEntry.logEntry;
    this.dispatchEvent(new ToolTipEvent(logEntry, e.target));
  }

  _handleFlameChartClick(e) {
    const profileNode = e.target.data;
    if (!profileNode) return;
    this._updateOverview(profileNode);
    this._updateFlameChart(profileNode)
  }
});
