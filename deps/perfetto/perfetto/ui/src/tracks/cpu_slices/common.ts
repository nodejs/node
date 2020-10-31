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

import {TrackData} from '../../common/track_data';

export const CPU_SLICE_TRACK_KIND = 'CpuSliceTrack';

export interface SummaryData extends TrackData {
  kind: 'summary';

  bucketSizeSeconds: number;
  utilizations: Float64Array;
}

export interface SliceData extends TrackData {
  kind: 'slice';

  // Slices are stored in a columnar fashion. All fields have the same length.
  ids: Float64Array;
  starts: Float64Array;
  ends: Float64Array;
  utids: Uint32Array;
}

export type Data = SummaryData | SliceData;


export interface Config { cpu: number; }
