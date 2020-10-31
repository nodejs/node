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

import {splitIfTooBig} from './flamegraph';

test('textGoingToMultipleLines', () => {
  const text = 'Dummy text to go to multiple lines.';

  const lineSplitter = splitIfTooBig(text, 7 + 32, text.length);

  expect(lineSplitter).toEqual({
    lines: ['Dummy t', 'ext to ', 'go to m', 'ultiple', ' lines.'],
    lineWidth: 7
  });
});

test('emptyText', () => {
  const text = '';

  const lineSplitter = splitIfTooBig(text, 10, 5);

  expect(lineSplitter).toEqual({lines: [], lineWidth: 5});
});

test('textEnoughForOneLine', () => {
  const text = 'Dummy text to go to one lines.';

  const lineSplitter = splitIfTooBig(text, text.length + 32, text.length);

  expect(lineSplitter).toEqual({lines: [text], lineWidth: text.length});
});

test('textGoingToTwoLines', () => {
  const text = 'Dummy text to go to two lines.';

  const lineSplitter = splitIfTooBig(text, text.length / 2 + 32, text.length);

  expect(lineSplitter).toEqual({
    lines: ['Dummy text to g', 'o to two lines.'],
    lineWidth: text.length / 2
  });
});
