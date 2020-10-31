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

import {assertTrue} from '../base/logging';

const EPSILON = 0.0000000001;

// TODO(hjd): Combine with timeToCode.
export function timeToString(sec: number) {
  const units = ['s', 'ms', 'us', 'ns'];
  const sign = Math.sign(sec);
  let n = Math.abs(sec);
  let u = 0;
  while (n < 1 && n !== 0 && u < units.length - 1) {
    n *= 1000;
    u++;
  }
  return `${sign < 0 ? '-' : ''}${Math.round(n * 10) / 10} ${units[u]}`;
}

export function fromNs(ns: number) {
  return ns / 1e9;
}

export function toNsFloor(seconds: number) {
  return Math.floor(seconds * 1e9);
}

export function toNsCeil(seconds: number) {
  return Math.ceil(seconds * 1e9);
}

export function toNs(seconds: number) {
  return Math.round(seconds * 1e9);
}

// 1000000023ns -> "1.000 000 023"
export function formatTimestamp(sec: number) {
  const parts = sec.toFixed(9).split('.');
  parts[1] = parts[1].replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
  return parts.join('.');
}

// TODO(hjd): Rename to formatTimestampWithUnits
// 1000000023ns -> "1s 23ns"
export function timeToCode(sec: number) {
  let result = '';
  let ns = Math.round(sec * 1e9);
  if (ns < 1) return '0s';
  const unitAndValue = [
    ['m', 60000000000],
    ['s', 1000000000],
    ['ms', 1000000],
    ['us', 1000],
    ['ns', 1]
  ];
  unitAndValue.forEach(pair => {
    const unit = pair[0] as string;
    const val = pair[1] as number;
    if (ns >= val) {
      const i = Math.floor(ns / val);
      ns -= i * val;
      result += i.toLocaleString() + unit + ' ';
    }
  });
  return result.slice(0, -1);
}

export class TimeSpan {
  readonly start: number;
  readonly end: number;

  constructor(start: number, end: number) {
    assertTrue(start <= end);
    this.start = start;
    this.end = end;
  }

  clone() {
    return new TimeSpan(this.start, this.end);
  }

  equals(other: TimeSpan): boolean {
    return Math.abs(this.start - other.start) < EPSILON &&
        Math.abs(this.end - other.end) < EPSILON;
  }

  get duration() {
    return this.end - this.start;
  }

  isInBounds(sec: number) {
    return this.start <= sec && sec <= this.end;
  }

  add(sec: number): TimeSpan {
    return new TimeSpan(this.start + sec, this.end + sec);
  }

  contains(other: TimeSpan) {
    return this.start <= other.start && other.end <= this.end;
  }
}
