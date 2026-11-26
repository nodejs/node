// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --scavenger-precise-object-pinning
// Overwriting the sampling interval causes a flag contradiction in
// variant == stress_sampling
// Flags: --flag-processing-mode=ignore-contradictions
// Flags: --stress-sampling-allocation-profiler=1

const oldHolder = [];

function makeConsStringsAndReturnFlats(iter) {
  const prefix = 'AAAAAAAAAAAA' + iter;
  const suffix = 'BBBBBBBBBBBB' + iter;
  const flats = [];
  for (let i = 0; i < 16; i++) {
    const c = prefix + i + suffix + i;
    const flat = c.toUpperCase();
    oldHolder.push(c);
    flats.push(flat);
  }
  return flats;
}

function pinFlatsAndScavenge() {
  gc({type: 'minor'});
}

for (let k = 0; k < 20; k++) {
  let flats = makeConsStringsAndReturnFlats(k);
  let [f] = flats;
  pinFlatsAndScavenge();
}
