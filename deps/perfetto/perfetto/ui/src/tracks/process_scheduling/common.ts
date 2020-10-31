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

import {TrackData} from '../../common/track_data';

export const PROCESS_SCHEDULING_TRACK_KIND = 'ProcessSchedulingTrack';

export interface SummaryData extends TrackData {
  kind: 'summary';

  bucketSizeSeconds: number;
  utilizations: Float64Array;
}

export interface SliceData extends TrackData {
  kind: 'slice';
  maxCpu: number;

  // Slices are stored in a columnar fashion. All fields have the same length.
  starts: Float64Array;
  ends: Float64Array;
  utids: Uint32Array;
  cpus: Uint32Array;
}

export type Data = SummaryData | SliceData;

export interface Config {
  pidForColor: number;
  upid: null|number;
  utid: number;
}
