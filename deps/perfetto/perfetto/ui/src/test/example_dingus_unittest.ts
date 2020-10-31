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

import {dingus} from 'dingusjs';

interface VoidFunctionWithNumberArg {
  (_: number): void;
}

class ExampleClass {
  isEven(n: number): boolean {
    return n % 2 === 0;
  }
}

function* evenNumbers(n: number, math: ExampleClass) {
  for (let i = 0; i < n; i++) {
    if (math.isEven(i)) yield i;
  }
}

/**
 * Call |f| |n| times (once with each number [0, n)).
 */
function iterMap(f: VoidFunctionWithNumberArg, n: number): void {
  for (let i = 0; i < n; i++) {
    f(i);
  }
}

test('example dingus test', () => {
  const d = dingus<VoidFunctionWithNumberArg>();
  iterMap(d, 3);
  expect(d.calls.length).toBe(3);
  expect(d.calls.filter(([_a, args, _b]) => args[0] === 1).length).toBe(1);
});

test('example dingus test class', () => {
  const d = dingus<ExampleClass>('math');
  // Dingus returns a truthy dingus in all situations - so bear that in mind!
  expect([...evenNumbers(3, d)]).toEqual([0, 1, 2]);
});
