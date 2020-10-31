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
  CPU_SLICE_TRACK_KIND,
  Data,
  SliceData,
  SummaryData
} from './common';

class CpuSliceTrackController extends TrackController<Config, Data> {
  static readonly kind = CPU_SLICE_TRACK_KIND;
  private setup = false;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);

    if (this.setup === false) {
      await this.query(
          `create virtual table ${this.tableName('window')} using window;`);
      await this.query(`create virtual table ${this.tableName('span')}
              using span_join(sched PARTITIONED cpu,
                              ${this.tableName('window')});`);
      this.setup = true;
    }

    const isQuantized = this.shouldSummarize(resolution);
    // |resolution| is in s/px we want # ns for 10px window:
    const bucketSizeNs = Math.round(resolution * 10 * 1e9);
    let windowStartNs = startNs;
    if (isQuantized) {
      windowStartNs = Math.floor(windowStartNs / bucketSizeNs) * bucketSizeNs;
    }
    const windowDurNs = Math.max(1, endNs - windowStartNs);

    this.query(`update ${this.tableName('window')} set
      window_start=${windowStartNs},
      window_dur=${windowDurNs},
      quantum=${isQuantized ? bucketSizeNs : 0}
      where rowid = 0;`);

    if (isQuantized) {
      return this.computeSummary(
          fromNs(windowStartNs), end, resolution, bucketSizeNs);
    } else {
      return this.computeSlices(fromNs(windowStartNs), end, resolution);
    }
  }

  private async computeSummary(
      start: number, end: number, resolution: number,
      bucketSizeNs: number): Promise<SummaryData> {
    const startNs = toNs(start);
    const endNs = toNs(end);
    const numBuckets = Math.ceil((endNs - startNs) / bucketSizeNs);

    const query = `select
        quantum_ts as bucket,
        sum(dur)/cast(${bucketSizeNs} as float) as utilization
        from ${this.tableName('span')}
        where cpu = ${this.config.cpu}
        and utid != 0
        group by quantum_ts limit ${LIMIT}`;

    const rawResult = await this.query(query);
    const numRows = +rawResult.numRecords;

    const summary: Data = {
      kind: 'summary',
      start,
      end,
      resolution,
      length: numRows,
      bucketSizeSeconds: fromNs(bucketSizeNs),
      utilizations: new Float64Array(numBuckets),
    };
    const cols = rawResult.columns;
    for (let row = 0; row < numRows; row++) {
      const bucket = +cols[0].longValues![row];
      summary.utilizations[bucket] = +cols[1].doubleValues![row];
    }
    return summary;
  }

  private async computeSlices(start: number, end: number, resolution: number):
      Promise<SliceData> {
    // TODO(hjd): Remove LIMIT
    const LIMIT = 10000;

    const query = `select ts,dur,utid,id from ${this.tableName('span')}
        where cpu = ${this.config.cpu}
        and utid != 0
        limit ${LIMIT};`;
    const rawResult = await this.query(query);

    const numRows = +rawResult.numRecords;
    const slices: SliceData = {
      kind: 'slice',
      start,
      end,
      resolution,
      length: numRows,
      ids: new Float64Array(numRows),
      starts: new Float64Array(numRows),
      ends: new Float64Array(numRows),
      utids: new Uint32Array(numRows),
    };

    const cols = rawResult.columns;
    for (let row = 0; row < numRows; row++) {
      const startSec = fromNs(+cols[0].longValues![row]);
      slices.starts[row] = startSec;
      slices.ends[row] = startSec + fromNs(+cols[1].longValues![row]);
      slices.utids[row] = +cols[2].longValues![row];
      slices.ids[row] = +cols[3].longValues![row];
    }
    if (numRows === LIMIT) {
      slices.end = slices.ends[slices.ends.length - 1];
    }
    return slices;
  }


  onDestroy(): void {
    if (this.setup) {
      this.query(`drop table ${this.tableName('window')}`);
      this.query(`drop table ${this.tableName('span')}`);
      this.setup = false;
    }
  }
}

trackControllerRegistry.register(CpuSliceTrackController);
