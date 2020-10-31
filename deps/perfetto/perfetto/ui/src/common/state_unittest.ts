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

import {createEmptyState, getContainingTrackId, State} from './state';

test('createEmptyState', () => {
  const state: State = createEmptyState();
  expect(state.nextId).toEqual(0);
});

test('getContainingTrackId', () => {
  const state: State = createEmptyState();
  state.tracks['a'] = {
    id: 'a',
    engineId: 'engine',
    kind: 'Foo',
    name: 'a track',
    config: {},
  };

  state.tracks['b'] = {
    id: 'b',
    engineId: 'engine',
    kind: 'Foo',
    name: 'b track',
    config: {},
    trackGroup: 'containsB',
  };

  expect(getContainingTrackId(state, 'z')).toEqual(null);
  expect(getContainingTrackId(state, 'a')).toEqual(null);
  expect(getContainingTrackId(state, 'b')).toEqual('containsB');
});
