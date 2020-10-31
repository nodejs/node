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

export const SLICE_TRACK_KIND = 'ChromeSliceTrack';

export interface Config {
  maxDepth: number;
  namespace: string;
  trackId: number;
}

export interface Data extends TrackData {
  // Slices are stored in a columnar fashion.
  strings: string[];
  sliceIds: Float64Array;
  starts: Float64Array;
  ends: Float64Array;
  depths: Uint16Array;
  titles: Uint16Array;  // Index into strings.

  // Start offset into into summary columns or -1 if not summarised.
  summarizedOffset: Int16Array;
  // Number of summary data points for this slice.
  summarizedSize: Uint16Array;

  // These arrays are length S where S is number of summarized slices * the
  // items in each slice.
  summaryNameId: Uint16Array;
  summaryPercent: Float64Array;
}