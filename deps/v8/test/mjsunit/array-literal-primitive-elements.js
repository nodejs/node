// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Array literal boilerplates of primitive elements.

function make() {
  return [
    [1, 2, 3],
    [1, , 3],
    [1, 2.5, 3],
    [1, , 2.5],
    [null, undefined, true, false],
    [0, null, , true, 'x'],
    [1, [2, null], 3],
    [1, x => x, 3],
  ];
}

// Expected values must not come from array literal boilerplates themselves.
// JSON has neither holes nor undefined: holes are given separately as indices,
// and the string 'undefined' stands for undefined.
const expected = [
  ['[1,2,3]', []],
  ['[1,null,3]', [1]],
  ['[1,2.5,3]', []],
  ['[1,null,2.5]', [1]],
  ['[null,"undefined",true,false]', []],
  ['[0,null,null,true,"x"]', [2]],
  ['[1,[2,null],3]', []],
];

// Later iterations check that mutating earlier results left the
// boilerplates intact.
for (let i = 0; i < 3; i++) {
  const arrays = make();
  // Element accesses and stores below transition elements kinds and feed
  // back into the allocation sites, so only the first copies reflect the
  // boilerplates' elements kinds.
  if (i == 0) {
    assertTrue(%HasSmiElements(arrays[0]));
    assertTrue(%HasHoleyElements(arrays[1]));
    assertTrue(%HasDoubleElements(arrays[2]));
    assertTrue(%HasDoubleElements(arrays[3]));
    assertTrue(%HasHoleyElements(arrays[3]));
    assertTrue(%HasObjectElements(arrays[4]));
    assertTrue(%HasObjectElements(arrays[5]));
    assertTrue(%HasHoleyElements(arrays[5]));
  }
  for (let j = 0; j < expected.length; j++) {
    const [json, holes] = expected[j];
    const values = JSON.parse(json);
    assertEquals(values.length, arrays[j].length);
    for (let k = 0; k < values.length; k++) {
      if (holes.includes(k)) {
        assertFalse(k in arrays[j]);
      } else {
        assertTrue(k in arrays[j]);
        const value = values[k] === 'undefined' ? undefined : values[k];
        assertEquals(value, arrays[j][k]);
      }
    }
  }
  assertEquals(42, arrays[7][1](42));

  // Mutating one result must not leak into the boilerplate.
  arrays[0][0] = 'changed';
  arrays[4][0] = 'changed';
  arrays[6][1][0] = 'changed';
}
