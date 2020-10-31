// Copyright (C) 2018 The Android Open Source Project
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
  COUNTER_TRACK_KIND,
  Data,
} from './common';

class CounterTrackController extends TrackController<Config, Data> {
  static readonly kind = COUNTER_TRACK_KIND;
  private setup = false;
  private maximumValueSeen = 0;
  private minimumValueSeen = 0;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);

    if (!this.setup) {
      if (this.config.namespace === undefined) {
        await this.query(`
          create view ${this.tableName('counter_view')} as
          select
            id,
            ts,
            dur,
            value
          from experimental_counter_dur
          where track_id = ${this.config.trackId};
        `);
      } else {
        await this.query(`
          create view ${this.tableName('counter_view')} as
          select
            id,
            ts,
            lead(ts, 1, ts) over (order by ts) - ts as dur,
            value
          from ${this.namespaceTable('counter')}
          where track_id = ${this.config.trackId};
        `);
      }

      const result = await this.query(`
        select max(value), min(value)
        from ${this.tableName('counter_view')}`);
      this.maximumValueSeen = +result.columns[0].doubleValues![0];
      this.minimumValueSeen = +result.columns[1].doubleValues![0];
      await this.query(
          `create virtual table ${this.tableName('window')} using window;`);

      await this.query(`create virtual table ${this.tableName('span')} using
        span_join(${this.tableName('counter_view')},
        ${this.tableName('window')});`);
      this.setup = true;
    }

    const result = await this.engine.queryOneRow(`
      select count(*)
      from ${this.tableName('counter_view')}
      where ts <= ${endNs} and ${startNs} <= ts + dur`);

    // Only quantize if we have too much data to draw.
    const isQuantized = result[0] > LIMIT;
    // |resolution| is in s/px we want # ns for 1px window:
    const bucketSizeNs = Math.round(resolution * 1e9);
    let windowStartNs = startNs;
    if (isQuantized) {
      windowStartNs = Math.floor(windowStartNs / bucketSizeNs) * bucketSizeNs;
    }
    const windowDurNs = Math.max(1, endNs - windowStartNs);

    this.query(`update ${this.tableName('window')} set
    window_start=${windowStartNs},
    window_dur=${windowDurNs},
    quantum=${isQuantized ? bucketSizeNs : 0}`);

    let query = `select min(ts) as ts,
      max(value) as value,
      -1 as id
      from ${this.tableName('span')}
      group by quantum_ts limit ${LIMIT};`;

    if (!isQuantized) {
      // Find the value just before the query range to ensure counters
      // continue from the correct previous value.
      // Union that with the query that finds all the counters within
      // the current query range.
      query = `
      select *
      from (
        select ts, value, id
        from ${this.tableName('counter_view')}
        where ts <= ${startNs}
        order by ts desc
        limit 1
      )
      union
      select *
      from (
        select ts, value, id
        from ${this.tableName('counter_view')}
        where ts <= ${endNs} and ${startNs} <= ts + dur
        limit ${LIMIT}
      );`;
    }

    const rawResult = await this.query(query);

    const numRows = +rawResult.numRecords;

    const data: Data = {
      start,
      end,
      length: numRows,
      isQuantized,
      maximumValue: this.maximumValue(),
      minimumValue: this.minimumValue(),
      resolution,
      timestamps: new Float64Array(numRows),
      values: new Float64Array(numRows),
      ids: new Float64Array(numRows),
    };

    const cols = rawResult.columns;
    for (let row = 0; row < numRows; row++) {
      const startSec = fromNs(+cols[0].longValues![row]);
      const value = +cols[1].doubleValues![row];
      const id = +cols[2].longValues![row];
      data.timestamps[row] = startSec;
      data.values[row] = value;
      data.ids[row] = id;
    }

    return data;
  }

  private maximumValue() {
    if (this.config.maximumValue === undefined) {
      return this.maximumValueSeen;
    } else {
      return this.config.maximumValue;
    }
  }

  private minimumValue() {
    if (this.config.minimumValue === undefined) {
      return this.minimumValueSeen;
    } else {
      return this.config.minimumValue;
    }
  }

}

trackControllerRegistry.register(CounterTrackController);
