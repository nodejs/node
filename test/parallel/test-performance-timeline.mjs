// This file may needs to be updated to wpt:
// https://github.com/web-platform-tests/wpt

import '../common/index.mjs';
import assert from 'assert';

import { performance } from 'perf_hooks';
import { setTimeout } from 'timers/promises';

// Order by startTime
performance.mark('one');
await setTimeout(50);
performance.mark('two');
await setTimeout(50);
performance.mark('three');
await setTimeout(50);
performance.measure('three', 'three');
await setTimeout(50);
performance.measure('two', 'two');
await setTimeout(50);
performance.measure('one', 'one');
const entries = performance.getEntriesByType('measure');
assert.deepStrictEqual(entries.map((x) => x.name), ['one', 'two', 'three']);
const allEntries = performance.getEntries();
assert.deepStrictEqual(allEntries.map((x) => x.name), ['one', 'one', 'two', 'two', 'three', 'three']);

performance.mark('a');
await setTimeout(50);
performance.measure('a', 'a');
await setTimeout(50);
performance.mark('a');
await setTimeout(50);
performance.measure('a', 'one');
const entriesByName = performance.getEntriesByName('a');
assert.deepStrictEqual(entriesByName.map((x) => x.entryType), ['measure', 'mark', 'measure', 'mark']);
