
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

import {Registry} from './registry';

interface Registrant {
  kind: string;
  n: number;
}

test('registry returns correct registrant', () => {
  const registry = new Registry<Registrant>();

  const a: Registrant = {kind: 'a', n: 1};
  const b: Registrant = {kind: 'b', n: 2};
  registry.register(a);
  registry.register(b);

  expect(registry.get('a')).toBe(a);
  expect(registry.get('b')).toBe(b);
});

test('registry throws error on kind collision', () => {
  const registry = new Registry<Registrant>();

  const a1: Registrant = {kind: 'a', n: 1};
  const a2: Registrant = {kind: 'a', n: 2};

  registry.register(a1);
  expect(() => registry.register(a2)).toThrow();
});

test('registry throws error on non-existent track', () => {
  const registry = new Registry<Registrant>();
  expect(() => registry.get('foo')).toThrow();
});
