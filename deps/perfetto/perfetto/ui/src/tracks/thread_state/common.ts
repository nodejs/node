
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

export const THREAD_STATE_TRACK_KIND = 'ThreadStateTrack';

export interface Data extends TrackData {
  strings: string[];
  starts: Float64Array;
  ends: Float64Array;
  state: Uint16Array;  // Index into |strings|.
  cpu: Uint8Array;
  summarisedStateBreakdowns: Map<number, StatePercent>;
}

export type StatePercent = Map<string, number>;

export interface Config {
  utid: number;
}
