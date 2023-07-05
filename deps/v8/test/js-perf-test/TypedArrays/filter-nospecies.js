// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SIZE = 1024;
let input;
let output;

function CreateSetup(TAConstructor) {
  return () => {
    // Create an Typed Array with a sequence of number 0 to SIZE.
    const values = Array.from({ length: SIZE }).map((_, i) =>
      TAConstructor === BigUint64Array ? BigInt(i) : i
    );
    input = new TAConstructor(values);
  };
}

// Creates a run function that is unpolluted by IC feedback.
function CreateRun() {
  // Filters out every other (odd indexed) elements.
  return new Function(`
    output = input.filter((el, i) => el < SIZE && (i % 2) === 0);
  `);
}

function isOutputInvalid() {
  if (output.length !== input.length / 2) return true;

  // Verfies every other (odd indexed) element has been filtered out.
  for (let i = 0; i < SIZE / 2; i++) {
    if (output[i] !== input[i * 2]) return true;
  }
}

function TearDown() {
  if (isOutputInvalid()) throw new TypeError(`Unexpected result!\n${output}`);

  input = void 0;
  output = void 0;
}

createSuite(
  'Uint8Array', 1000, CreateRun(), CreateSetup(Uint8Array), TearDown);
createSuite(
  'Uint16Array', 1000, CreateRun(), CreateSetup(Uint16Array), TearDown);
createSuite(
  'Uint32Array', 1000, CreateRun(), CreateSetup(Uint32Array), TearDown);
createSuite(
  'Float32Array', 1000, CreateRun(), CreateSetup(Float32Array), TearDown);
createSuite(
  'Float64Array', 1000, CreateRun(), CreateSetup(Float64Array), TearDown);
createSuite(
  'BigUint64Array', 1000, CreateRun(), CreateSetup(BigUint64Array),
  TearDown);
