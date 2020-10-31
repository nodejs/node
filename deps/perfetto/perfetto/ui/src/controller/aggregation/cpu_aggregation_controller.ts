// Copyright (C) 2020 The Android Open Source Project
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

import {ColumnDef} from '../../common/aggregation_data';
import {Engine} from '../../common/engine';
import {Sorting, TimestampedAreaSelection} from '../../common/state';
import {toNs} from '../../common/time';
import {Config, CPU_SLICE_TRACK_KIND} from '../../tracks/cpu_slices/common';
import {globals} from '../globals';

import {AggregationController} from './aggregation_controller';


export class CpuAggregationController extends AggregationController {
  async createAggregateView(
      engine: Engine, selectedArea: TimestampedAreaSelection) {
    await engine.query(`drop view if exists ${this.kind};`);
    const area = selectedArea.area;
    if (area === undefined) return false;

    const selectedCpus = [];
    for (const trackId of area.tracks) {
      const track = globals.state.tracks[trackId];
      // Track will be undefined for track groups.
      if (track !== undefined && track.kind === CPU_SLICE_TRACK_KIND) {
        selectedCpus.push((track.config as Config).cpu);
      }
    }
    if (selectedCpus.length === 0) return false;

    const query = `create view ${this.kind} as
        SELECT process.name as process_name, pid, thread.name as thread_name,
        tid, sum(dur) AS total_dur,
        sum(dur)/count(1) as avg_dur,
        count(1) as occurrences
        FROM process
        JOIN thread USING(upid)
        JOIN thread_state USING(utid)
        WHERE cpu IN (${selectedCpus}) AND
        state = "Running" AND
        thread_state.ts + thread_state.dur > ${toNs(area.startSec)} AND
        thread_state.ts < ${toNs(area.endSec)} group by utid`;

    await engine.query(query);
    return true;
  }

  getTabName() {
    return 'CPU Slices';
  }

  getDefaultSorting(): Sorting {
    return {column: 'total_dur', direction: 'DESC'};
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Process',
        kind: 'STRING',
        columnConstructor: Uint16Array,
        columnId: 'process_name',
      },
      {
        title: 'PID',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'pid'
      },
      {
        title: 'Thread',
        kind: 'STRING',
        columnConstructor: Uint16Array,
        columnId: 'thread_name'
      },
      {
        title: 'TID',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'tid'
      },
      {
        title: 'Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'total_dur'
      },
      {
        title: 'Avg Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'avg_dur'
      },
      {
        title: 'Occurrences',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'occurrences'
      }
    ];
  }
}
