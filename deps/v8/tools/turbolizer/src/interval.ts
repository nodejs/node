// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class Interval {
  start: number;
  end: number;

  constructor(numbers: [number, number]) {
    this.start = numbers[0];
    this.end = numbers[1];
  }
}
