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

import {fromNs, toNs} from '../../common/time';
import {LIMIT} from '../../common/track_data';

import {
  TrackController,
  trackControllerRegistry
} from '../../controller/track_controller';

import {
  Config,
  Data,
  THREAD_STATE_TRACK_KIND,
} from './common';

class ThreadStateTrackController extends TrackController<Config, Data> {
  static readonly kind = THREAD_STATE_TRACK_KIND;
  private setup = false;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);
    const minNs = Math.round(resolution * 1e9);

    await this.query(
        `drop view if exists ${this.tableName('grouped_thread_states')}`);

    // This query gives all contiguous slices less than minNs the same grouping.
    await this.query(`create view ${this.tableName('grouped_thread_states')} as
    select ts, dur, cpu, state,
    ifnull(sum(value) over (order by ts), 0) as grouping
    from
    (select *,
    (dur >= ${minNs}) or lag(dur >= ${minNs}) over (order by ts) as value
    from thread_state
    where utid = ${this.config.utid})`);

    // Since there are more rows than slices we will output, check the number of
    // distinct groupings to find the number of slices.
    const totalSlicesQuery = `select count(distinct(grouping))
      from ${this.tableName('grouped_thread_states')}
      where ts <= ${endNs} and ts + dur >= ${startNs}`;
    const totalSlices = (await this.engine.queryOneRow(totalSlicesQuery))[0];

    // We have ts contraints instead of span joining with the window table
    // because when selecting a slice we need the real duration (even if it
    // is outside of the current viewport)
    // TODO(b/149303809): Return this to using
    // the window table if possible.
    const query = `select min(min(ts)) over (partition by grouping) as ts,
    sum(sum(dur)) over (partition by grouping) as slice_dur,
    cpu,
    state,
    (sum(dur) * 1.0)/(sum(sum(dur)) over (partition by grouping)) as percent,
    grouping
    from ${this.tableName('grouped_thread_states')}
    where ts <= ${endNs} and ts + dur >= ${startNs}
    group by grouping, state
    order by grouping
    limit ${LIMIT}`;

    const result = await this.query(query);
    const numRows = +result.numRecords;

    const summary: Data = {
      start,
      end,
      resolution,
      length: totalSlices,
      starts: new Float64Array(totalSlices),
      ends: new Float64Array(totalSlices),
      strings: [],
      state: new Uint16Array(totalSlices),
      cpu: new Uint8Array(totalSlices),
      summarisedStateBreakdowns: new Map(),
    };

    const stringIndexes = new Map<string, number>();
    function internString(str: string) {
      let idx = stringIndexes.get(str);
      if (idx !== undefined) return idx;
      idx = summary.strings.length;
      summary.strings.push(str);
      stringIndexes.set(str, idx);
      return idx;
    }

    let outIndex = 0;
    for (let row = 0; row < numRows; row++) {
      const cols = result.columns;
      const start = +cols[0].longValues![row];
      const dur = +cols[1].longValues![row];
      const state = cols[3].stringValues![row];
      const percent = +(cols[4].doubleValues![row].toFixed(2));
      const grouping = +cols[5].longValues![row];

      if (percent !== 1) {
        let breakdownMap = summary.summarisedStateBreakdowns.get(outIndex);
        if (!breakdownMap) {
          breakdownMap = new Map();
          summary.summarisedStateBreakdowns.set(outIndex, breakdownMap);
        }

        const currentPercent = breakdownMap.get(state);
        if (currentPercent === undefined) {
          breakdownMap.set(state, percent);
        } else {
          breakdownMap.set(state, currentPercent + percent);
        }
      }

      const nextGrouping =
          row + 1 < numRows ? +cols[5].longValues![row + 1] : -1;
      // If the next grouping is different then we have reached the end of this
      // slice.
      if (grouping !== nextGrouping) {
        const numStates = summary.summarisedStateBreakdowns.get(outIndex) ?
            summary.summarisedStateBreakdowns.get(outIndex)!.entries.length :
            1;
        summary.starts[outIndex] = fromNs(start);
        summary.ends[outIndex] = fromNs(start + dur);
        summary.state[outIndex] =
            internString(numStates === 1 ? state : 'Various states');
        summary.cpu[outIndex] = +cols[2].longValues![row];
        outIndex++;
      }
    }
    return summary;
  }

  onDestroy(): void {
    if (this.setup) {
      this.query(`drop view ${this.tableName('grouped_thread_states')}`);
      this.setup = false;
    }
  }
}

trackControllerRegistry.register(ThreadStateTrackController);
