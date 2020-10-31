// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {assertExists} from '../base/logging';
import {Actions} from '../common/actions';
import {
  LogBounds,
  LogBoundsKey,
  LogEntries,
  LogEntriesKey
} from '../common/logs';
import {formatTimestamp} from '../common/time';
import {TimeSpan} from '../common/time';

import {globals} from './globals';
import {Panel} from './panel';

const ROW_H = 20;

const PRIO_TO_LETTER = ['-', '-', 'V', 'D', 'I', 'W', 'E', 'F'];

export class LogPanel extends Panel<{}> {
  private scrollContainer?: HTMLElement;
  private bounds?: LogBounds;
  private entries?: LogEntries;

  private visibleRowOffset = 0;
  private visibleRowCount = 0;

  recomputeVisibleRowsAndUpdate() {
    const scrollContainer = assertExists(this.scrollContainer);

    const prevOffset = this.visibleRowOffset;
    const prevCount = this.visibleRowCount;
    this.visibleRowOffset = Math.floor(scrollContainer.scrollTop / ROW_H);
    this.visibleRowCount = Math.ceil(scrollContainer.clientHeight / ROW_H);

    if (this.visibleRowOffset !== prevOffset ||
        this.visibleRowCount !== prevCount)
       {
        globals.dispatch(Actions.updateLogsPagination({
          offset: this.visibleRowOffset,
          count: this.visibleRowCount,
        }));
      }
  }

  oncreate({dom}: m.CVnodeDOM) {
    this.scrollContainer = assertExists(
        dom.parentElement!.parentElement!.parentElement as HTMLElement);
    this.scrollContainer.addEventListener(
        'scroll', this.onScroll.bind(this), {passive: true});
    this.bounds = globals.trackDataStore.get(LogBoundsKey) as LogBounds;
    this.entries = globals.trackDataStore.get(LogEntriesKey) as LogEntries;
    this.recomputeVisibleRowsAndUpdate();
  }

  onupdate(_: m.CVnodeDOM) {
    this.bounds = globals.trackDataStore.get(LogBoundsKey) as LogBounds;
    this.entries = globals.trackDataStore.get(LogEntriesKey) as LogEntries;
    this.recomputeVisibleRowsAndUpdate();
  }

  onScroll() {
    if (this.scrollContainer === undefined) return;
    this.recomputeVisibleRowsAndUpdate();
    globals.rafScheduler.scheduleFullRedraw();
  }

  onRowOver(ts: number) {
    globals.frontendLocalState.setHoveredLogsTimestamp(ts);
  }

  onRowOut() {
    globals.frontendLocalState.setHoveredLogsTimestamp(-1);
  }

  private totalRows():
      {isStale: boolean, total: number, offset: number, count: number} {
    if (!this.bounds) {
      return {isStale: false, total: 0, offset: 0, count: 0};
    }
    const {total, startTs, endTs, firstRowTs, lastRowTs} = this.bounds;
    const vis = globals.frontendLocalState.visibleWindowTime;
    const leftSpan = new TimeSpan(startTs, firstRowTs);
    const rightSpan = new TimeSpan(lastRowTs, endTs);

    const isStaleLeft = !leftSpan.isInBounds(vis.start);
    const isStaleRight = !rightSpan.isInBounds(vis.end);
    const isStale = isStaleLeft || isStaleRight;
    const offset = Math.min(this.visibleRowOffset, total);
    const visCount = Math.min(total, this.visibleRowCount);
    return {isStale, total, count: visCount, offset};
  }

  view(_: m.CVnode<{}>) {
    const {isStale, total, offset, count} = this.totalRows();

    const rows: m.Children = [];
    if (this.entries) {
      const offset = this.entries.offset;
      const timestamps = this.entries.timestamps;
      const priorities = this.entries.priorities;
      const tags = this.entries.tags;
      const messages = this.entries.messages;
      for (let i = 0; i < this.entries.timestamps.length; i++) {
        const priorityLetter = PRIO_TO_LETTER[priorities[i]];
        const ts = timestamps[i];
        const prioClass = priorityLetter || '';
        rows.push(
            m(`.row.${prioClass}`,
              {
                'class': isStale ? 'stale' : '',
                style: {top: `${(offset + i) * ROW_H}px`},
                onmouseover: this.onRowOver.bind(this, ts / 1e9),
                onmouseout: this.onRowOut.bind(this),
              },
              m('.cell',
                formatTimestamp(ts / 1e9 - globals.state.traceTime.startSec)),
              m('.cell', priorityLetter || '?'),
              m('.cell', tags[i]),
              m('.cell', messages[i])));
      }
    }

    return m(
        '.log-panel',
        m('header',
          {
            'class': isStale ? 'stale' : '',
          },
          `Logs rows [${offset}, ${offset + count}] / ${total}`),
        m('.rows', {style: {height: `${total * ROW_H}px`}}, rows));
  }

  renderCanvas() {}
}
