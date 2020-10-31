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

import {TimeSpan, timeToCode} from './time';

test('seconds to code', () => {
  expect(timeToCode(3)).toEqual('3s');
  expect(timeToCode(60)).toEqual('1m');
  expect(timeToCode(63)).toEqual('1m 3s');
  expect(timeToCode(63.2)).toEqual('1m 3s 200ms');
  expect(timeToCode(63.2221)).toEqual('1m 3s 222ms 100us');
  expect(timeToCode(63.2221111)).toEqual('1m 3s 222ms 111us 100ns');
  expect(timeToCode(0.2221111)).toEqual('222ms 111us 100ns');
  expect(timeToCode(0.000001)).toEqual('1us');
  expect(timeToCode(0.000003)).toEqual('3us');
  expect(timeToCode(1.000001)).toEqual('1s 1us');
  expect(timeToCode(200.00000003)).toEqual('3m 20s 30ns');
  expect(timeToCode(0)).toEqual('0s');
});

test('Time span equality', () => {
  expect((new TimeSpan(0, 1)).equals(new TimeSpan(0, 1))).toBe(true);
  expect((new TimeSpan(0, 1)).equals(new TimeSpan(0, 2))).toBe(false);
  expect((new TimeSpan(0, 1)).equals(new TimeSpan(0, 1 + Number.EPSILON)))
      .toBe(true);
});
