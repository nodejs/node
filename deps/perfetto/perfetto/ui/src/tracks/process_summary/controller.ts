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
  Data,
  PROCESS_SUMMARY_TRACK,
} from './common';

// This is the summary displayed when a process only contains chrome slices
// and no cpu scheduling.

class ProcessSummaryTrackController extends TrackController<Config, Data> {
  static readonly kind = PROCESS_SUMMARY_TRACK;
  private setup = false;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);

    if (this.setup === false) {
      await this.query(
          `create virtual table ${this.tableName('window')} using window;`);

      let utids = [this.config.utid];
      if (this.config.upid) {
        const threadQuery = await this.query(
            `select utid from thread where upid=${this.config.upid}`);
        utids = threadQuery.columns[0].longValues! as number[];
      }

      const trackQuery = await this.query(
          `select id from thread_track where utid in (${utids.join(',')})`);
      const tracks = trackQuery.columns[0].longValues! as number[];

      const processSliceView = this.tableName('process_slice_view');
      await this.query(
          `create view ${processSliceView} as ` +
          // 0 as cpu is a dummy column to perform span join on.
          `select ts, dur/${utids.length} as dur ` +
          `from slice s ` +
          `where depth = 0 and track_id in ` +
          `(${tracks.join(',')})`);
      await this.query(`create virtual table ${this.tableName('span')}
          using span_join(${processSliceView},
                          ${this.tableName('window')});`);
      this.setup = true;
    }

    // |resolution| is in s/px we want # ns for 10px window:
    // Max value with 1 so we don't end up with resolution 0.
    const bucketSizeNs = Math.max(1, Math.round(resolution * 10 * 1e9));
    const windowStartNs = Math.floor(startNs / bucketSizeNs) * bucketSizeNs;
    const windowDurNs = Math.max(1, endNs - windowStartNs);

    this.query(`update ${this.tableName('window')} set
      window_start=${windowStartNs},
      window_dur=${windowDurNs},
      quantum=${bucketSizeNs}
      where rowid = 0;`);

    return this.computeSummary(
        fromNs(windowStartNs), end, resolution, bucketSizeNs);
  }

  private async computeSummary(
      start: number, end: number, resolution: number,
      bucketSizeNs: number): Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);
    const numBuckets =
        Math.min(Math.ceil((endNs - startNs) / bucketSizeNs), LIMIT);

    const query = `select
      quantum_ts as bucket,
      sum(dur)/cast(${bucketSizeNs} as float) as utilization
      from ${this.tableName('span')}
      group by quantum_ts
      limit ${LIMIT}`;

    const rawResult = await this.query(query);
    const numRows = +rawResult.numRecords;

    const summary: Data = {
      start,
      end,
      resolution,
      length: numBuckets,
      bucketSizeSeconds: fromNs(bucketSizeNs),
      utilizations: new Float64Array(numBuckets),
    };
    const cols = rawResult.columns;
    for (let row = 0; row < numRows; row++) {
      const bucket = +cols[0].longValues![row];
      if (bucket > numBuckets) {
        continue;
      }
      summary.utilizations[bucket] = +cols[1].doubleValues![row];
    }
    return summary;
  }

  onDestroy(): void {
    if (this.setup) {
      this.query(`drop table ${this.tableName('window')}`);
      this.query(`drop table ${this.tableName('span')}`);
      this.setup = false;
    }
  }
}

trackControllerRegistry.register(ProcessSummaryTrackController);
