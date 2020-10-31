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
  trackControllerRegistry,
} from '../../controller/track_controller';

import {Config, Data, SLICE_TRACK_KIND} from './common';

class ChromeSliceTrackController extends TrackController<Config, Data> {
  static readonly kind = SLICE_TRACK_KIND;
  private setup = false;

  async onBoundsChange(start: number, end: number, resolution: number):
      Promise<Data> {
    const startNs = toNs(start);
    const endNs = toNs(end);
    // Ns in 1px width. We want all slices smaller than 1px to be grouped.
    const minNs = toNs(resolution);

    if (!this.setup) {
      await this.query(
          `create virtual table ${this.tableName('window')} using window;`);

      await this.query(`create view ${this.tableName('small')} as
        select ts, dur, depth, name, id as slice_id
        from ${this.namespaceTable('slice')}
        where track_id = ${this.config.trackId}
        and dur < ${minNs}
        order by ts;`);

      await this.query(`create virtual table ${this.tableName('span')} using
      span_join(${this.tableName('small')} PARTITIONED depth,
      ${this.tableName('window')});`);

      this.setup = true;
    }

    const windowDurNs = Math.max(1, endNs - startNs);

    this.query(`update ${this.tableName('window')} set
    window_start=${startNs},
    window_dur=${windowDurNs},
    quantum=${minNs}`);

    await this.query(`drop view if exists ${this.tableName('small')}`);
    await this.query(`drop view if exists ${this.tableName('big')}`);
    await this.query(`drop view if exists ${this.tableName('summary')}`);

    await this.query(`create view ${this.tableName('small')} as
      select ts, dur, depth, name, id as slice_id
      from ${this.namespaceTable('slice')}
      where track_id = ${this.config.trackId}
      and dur < ${minNs}
      order by ts`);

    await this.query(`create view ${this.tableName('big')} as
      select ts, dur, depth, name, id as slice_id, 1.0 as percent,
      -1 as grouping
      from ${this.namespaceTable('slice')}
      where track_id = ${this.config.trackId}
      and ts >= ${startNs} - dur
      and ts <= ${endNs}
      and dur >= ${minNs}
      order by ts `);

    await this.query(`create view ${this.tableName('summary')} as select
      min(min(ts)) over (partition by depth, quantum_ts) as ts,
      sum(sum(dur)) over (partition by depth, quantum_ts) as dur,
      depth,
      name,
      slice_id,
      (sum(dur) * 1.0)/(sum(sum(dur)) over
      (partition by depth,quantum_ts)) as percent,
      quantum_ts as grouping
      from ${this.tableName('span')}
      group by depth, quantum_ts, name
      order by ts;`);

    // Since there are more rows than slices we will output, check the number of
    // distinct groupings to find the number of slices.
    const totalSlicesQuery = `select (
      (select count(1) from ${this.tableName('big')}) +
      (select count(1) from (select distinct grouping, depth
      from ${this.tableName('summary')})))`;
    const totalSlices = (await this.engine.queryOneRow(totalSlicesQuery))[0];

    const totalSummarizedQuery =
        `select count(1) from ${this.tableName('summary')}`;
    const totalSummarized =
        (await this.engine.queryOneRow(totalSummarizedQuery))[0];

    const query = `select * from ${this.tableName('summary')} UNION
      select * from ${this.tableName('big')} order by ts, percent desc limit ${
        LIMIT}`;
    const result = await this.query(query);

    const numRows = +result.numRecords;

    const data: Data = {
      start,
      end,
      resolution,
      length: totalSlices,
      strings: [],
      sliceIds: new Float64Array(totalSlices),
      starts: new Float64Array(totalSlices),
      ends: new Float64Array(totalSlices),
      depths: new Uint16Array(totalSlices),
      titles: new Uint16Array(totalSlices),
      summarizedOffset: new Int16Array(totalSlices).fill(-1),
      summarizedSize: new Uint16Array(totalSlices),
      summaryNameId: new Uint16Array(totalSummarized),
      summaryPercent: new Float64Array(totalSummarized)
    };

    const stringIndexes = new Map<string, number>();
    function internString(str: string) {
      let idx = stringIndexes.get(str);
      if (idx !== undefined) return idx;
      idx = data.strings.length;
      data.strings.push(str);
      stringIndexes.set(str, idx);
      return idx;
    }

    let outIndex = 0;
    let summaryIndex = 0;
    const internedVarious = internString('Various slices');
    for (let row = 0; row < numRows; row++) {
      const cols = result.columns;
      const start = +cols[0].longValues![row];
      const dur = +cols[1].longValues![row];
      const depth = +cols[2].longValues![row];
      const name = cols[3].stringValues![row];
      const percent = +(cols[5].doubleValues![row].toFixed(2));
      const grouping = +cols[6].longValues![row];

      // If it is a summarized slice, store the slice percentage breakdown.
      if (percent !== 1) {
        if (data.summarizedOffset[outIndex] === -1) {
          data.summarizedOffset[outIndex] = summaryIndex;
        }
        data.summarizedSize[outIndex] = data.summarizedSize[outIndex] + 1;
        data.summaryNameId[summaryIndex] = internString(name);
        data.summaryPercent[summaryIndex] = percent;
        summaryIndex++;
      }

      const nextGrouping =
          row + 1 < numRows ? +cols[6].longValues![row + 1] : -1;
      const nextDepth = row + 1 < numRows ? +cols[2].longValues![row + 1] : -1;
      // If the next grouping or next depth is different then we have reached
      // the end of this slice.
      if (grouping === -1 || grouping !== nextGrouping || depth !== nextDepth) {
        const numSummarized = data.summarizedSize[outIndex];
        data.starts[outIndex] = fromNs(start);
        data.ends[outIndex] = fromNs(start + dur);
        data.titles[outIndex] =
            numSummarized > 0 ? internedVarious : internString(name);
        data.depths[outIndex] = depth;
        data.sliceIds[outIndex] = +cols[4].longValues![row];
        outIndex++;
      }
    }
    return data;
  }

}


trackControllerRegistry.register(ChromeSliceTrackController);
