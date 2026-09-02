// Flags: --expose-gc
// Label materialisation must define own properties without invoking inherited
// setters.
'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

// Poison the prototype setter for the label key and for every own-property
// name getAllocationProfile() writes onto its sample, entry, and result
// objects. All of those must be defined with CreateDataProperty, so none of
// these setters may run.
const POISONED_KEYS = [
  'route', 'labels', 'nodeId', 'size', 'count', 'sampleId', 'samples',
  'externalBytes', 'bytes',
];
for (const key of POISONED_KEYS) {
  Object.defineProperty(Object.prototype, key, {
    set() { throw new Error(`boom: ${key}`); },
    configurable: true,
  });
}

const EXTERNAL_BYTES = 1024 * 1024;
let buf;

try {
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  v8.withHeapProfileLabels({ route: '/x' }, () => {
    const arr = [];
    for (let i = 0; i < 10000; i++) arr.push({ data: i });
    // Allocate an external backing store so externalBytes entries are built
    // (their labels/bytes fields also go through CreateDataProperty). It is
    // kept alive in an outer binding until after the profile is read, so the
    // entry cannot vanish to a GC before getAllocationProfile() runs.
    buf = Buffer.allocUnsafeSlow(EXTERNAL_BYTES);
  });

  let profile;
  // Must not throw and must not abort the process.
  assert.doesNotThrow(() => {
    profile = handle.getAllocationProfile();
  });
  handle.stop();

  assert.ok(profile, 'getAllocationProfile() must return a profile object');
  assert.ok(Array.isArray(profile.samples), 'profile.samples must be an array');
  assert.ok(profile.samples.length > 0, 'must have at least one sample');

  // Every sample must have a labels field.
  for (const sample of profile.samples) {
    assert.strictEqual(typeof sample.labels, 'object');
    assert.ok(sample.labels !== null);
  }

  // Label materialisation must bypass the poisoned setter.
  const labeled = profile.samples.filter((s) => s.labels.route === '/x');
  assert.ok(
    labeled.length > 0,
    'Samples under withHeapProfileLabels({ route: "/x" }) must carry ' +
    'labels.route === "/x" — if this fails the poisoned setter ran'
  );

  // The externalBytes path builds its own objects with the poisoned keys
  // 'externalBytes', 'labels' and 'bytes'; assert it actually ran and
  // attributed the backing store, otherwise this test would silently stop
  // covering it.
  assert.ok(Array.isArray(profile.externalBytes),
            'externalBytes must be present: a labelled backing store is live');
  const external = profile.externalBytes.filter((e) => e.labels.route === '/x');
  assert.strictEqual(external.length, 1,
                     'Expected exactly one externalBytes entry for ' +
                     `route="/x", got ${external.length}`);
  assert.ok(external[0].bytes >= EXTERNAL_BYTES,
            `Expected >= ${EXTERNAL_BYTES} external bytes for route="/x", ` +
            `got ${external[0].bytes}`);

  // Keep the backing store alive across the profile read above.
  assert.strictEqual(buf.length, EXTERNAL_BYTES);
} finally {
  // Clean up the prototype mutations so other tests are not affected.
  for (const key of POISONED_KEYS) delete Object.prototype[key];
}
