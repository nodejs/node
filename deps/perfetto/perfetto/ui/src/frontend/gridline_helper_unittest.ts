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

import {getGridStepSize} from './gridline_helper';

test('gridline helper to have sensible step sizes', () => {
  expect(getGridStepSize(10, 14)).toEqual(1);
  expect(getGridStepSize(30, 14)).toEqual(2);
  expect(getGridStepSize(60, 14)).toEqual(5);
  expect(getGridStepSize(100, 14)).toEqual(10);

  expect(getGridStepSize(10, 21)).toEqual(0.5);
  expect(getGridStepSize(30, 21)).toEqual(2);
  expect(getGridStepSize(60, 21)).toEqual(2);
  expect(getGridStepSize(100, 21)).toEqual(5);

  expect(getGridStepSize(10, 3)).toEqual(5);
  expect(getGridStepSize(30, 3)).toEqual(10);
  expect(getGridStepSize(60, 3)).toEqual(20);
  expect(getGridStepSize(100, 3)).toEqual(50);

  expect(getGridStepSize(800, 4)).toEqual(200);
});

test('gridline helper to scale to very small and very large values', () => {
  expect(getGridStepSize(.01, 14)).toEqual(.001);
  expect(getGridStepSize(10000, 14)).toEqual(1000);
});

test('gridline helper to always return a reasonable number of steps', () => {
  for (let i = 1; i <= 1000; i++) {
    const stepSize = getGridStepSize(i, 14);
    expect(Math.round(i / stepSize)).toBeGreaterThanOrEqual(7);
    expect(Math.round(i / stepSize)).toBeLessThanOrEqual(21);
  }
});
