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

import {TimeSpan} from '../common/time';

import {computeZoom, TimeScale} from './time_scale';

test('time scale to work', () => {
  const scale = new TimeScale(new TimeSpan(0, 100), [200, 1000]);

  expect(scale.timeToPx(0)).toEqual(200);
  expect(scale.timeToPx(100)).toEqual(1000);
  expect(scale.timeToPx(50)).toEqual(600);

  expect(scale.pxToTime(200)).toEqual(0);
  expect(scale.pxToTime(1000)).toEqual(100);
  expect(scale.pxToTime(600)).toEqual(50);

  expect(scale.deltaPxToDuration(400)).toEqual(50);

  expect(scale.timeInBounds(50)).toEqual(true);
  expect(scale.timeInBounds(0)).toEqual(true);
  expect(scale.timeInBounds(100)).toEqual(true);
  expect(scale.timeInBounds(-1)).toEqual(false);
  expect(scale.timeInBounds(101)).toEqual(false);
});


test('time scale to be updatable', () => {
  const scale = new TimeScale(new TimeSpan(0, 100), [100, 1000]);

  expect(scale.timeToPx(0)).toEqual(100);

  scale.setLimitsPx(200, 1000);
  expect(scale.timeToPx(0)).toEqual(200);
  expect(scale.timeToPx(100)).toEqual(1000);

  scale.setTimeBounds(new TimeSpan(0, 200));
  expect(scale.timeToPx(0)).toEqual(200);
  expect(scale.timeToPx(100)).toEqual(600);
  expect(scale.timeToPx(200)).toEqual(1000);
});

test('it zooms', () => {
  const span = new TimeSpan(0, 20);
  const scale = new TimeScale(span, [0, 100]);
  const newSpan = computeZoom(scale, span, 0.5, 50);
  expect(newSpan.start).toEqual(5);
  expect(newSpan.end).toEqual(15);
});

test('it zooms an offset scale and span', () => {
  const span = new TimeSpan(1000, 1020);
  const scale = new TimeScale(span, [200, 300]);
  const newSpan = computeZoom(scale, span, 0.5, 250);
  expect(newSpan.start).toEqual(1005);
  expect(newSpan.end).toEqual(1015);
});

test('it clamps zoom in', () => {
  const span = new TimeSpan(1000, 1040);
  const scale = new TimeScale(span, [200, 300]);
  const newSpan = computeZoom(scale, span, 0.0000000001, 225);
  expect((newSpan.end - newSpan.start) / 2 + newSpan.start).toBeCloseTo(1010);
  expect(newSpan.end - newSpan.start).toBeCloseTo(1e-4, 8);
});
