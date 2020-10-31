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

import {hsl} from 'color-convert';
import {translateState} from '../common/thread_state';
import {ThreadDesc} from './globals';

export interface Color {
  c: string;
  h: number;
  s: number;
  l: number;
}

const MD_PALETTE: Color[] = [
  {c: 'red', h: 4, s: 90, l: 58},
  {c: 'pink', h: 340, s: 82, l: 52},
  {c: 'purple', h: 291, s: 64, l: 42},
  {c: 'deep purple', h: 262, s: 52, l: 47},
  {c: 'indigo', h: 231, s: 48, l: 48},
  {c: 'blue', h: 207, s: 90, l: 54},
  {c: 'light blue', h: 199, s: 98, l: 48},
  {c: 'cyan', h: 187, s: 100, l: 42},
  {c: 'teal', h: 174, s: 100, l: 29},
  {c: 'green', h: 122, s: 39, l: 49},
  {c: 'light green', h: 88, s: 50, l: 53},
  {c: 'lime', h: 66, s: 70, l: 54},
  {c: 'amber', h: 45, s: 100, l: 51},
  {c: 'orange', h: 36, s: 100, l: 50},
  {c: 'deep orange', h: 14, s: 100, l: 57},
  {c: 'brown', h: 16, s: 25, l: 38},
  {c: 'blue gray', h: 200, s: 18, l: 46},
  {c: 'yellow', h: 54, s: 100, l: 62},
];

const GREY_COLOR: Color = {
  c: 'grey',
  h: 0,
  s: 0,
  l: 62
};

function hash(s: string, max: number): number {
  let hash = 0x811c9dc5 & 0xfffffff;
  for (let i = 0; i < s.length; i++) {
    hash ^= s.charCodeAt(i);
    hash = (hash * 16777619) & 0xffffffff;
  }
  return Math.abs(hash) % max;
}

export function hueForCpu(cpu: number): number {
  return (128 + (32 * cpu)) % 256;
}

const DARK_GREEN: Color = {
  c: 'dark green',
  h: 120,
  s: 44,
  l: 34
};
const LIME_GREEN: Color = {
  c: 'lime green',
  h: 75,
  s: 55,
  l: 47
};
const LIGHT_GREY: Color = {
  c: 'light grey',
  h: 0,
  s: 0,
  l: 87
};
const ORANGE: Color = {
  c: 'orange',
  h: 36,
  s: 100,
  l: 50
};
const INDIGO: Color = {
  c: 'indigo',
  h: 231,
  s: 48,
  l: 48
};

export function colorForState(stateCode: string): Readonly<Color> {
  const state = translateState(stateCode);
  if (state === 'Running') {
    return DARK_GREEN;
  } else if (state.startsWith('Runnable')) {
    return LIME_GREEN;
  } else if (state.includes('Uninterruptible Sleep')) {
    return ORANGE;
  } else if (state.includes('Sleeping')) {
    return LIGHT_GREY;
  }
  return INDIGO;
}

export function colorForTid(tid: number): Color {
  const colorIdx = hash(tid.toString(), MD_PALETTE.length);
  return Object.assign({}, MD_PALETTE[colorIdx]);
}

export function colorForThread(thread: ThreadDesc|undefined): Color {
  if (thread === undefined) {
    return Object.assign({}, GREY_COLOR);
  }
  const tid = thread.pid ? thread.pid : thread.tid;
  return colorForTid(tid);
}

// 40 different random hues 9 degrees apart.
export function randomColor(): string {
  const hue = Math.floor(Math.random() * 40) * 9;
  return '#' + hsl.hex([hue, 90, 30]);
}
