import '../common/index.mjs';
import assert from 'assert';
import { Worker } from 'worker_threads';

const re = /The argument 'options\.eval' must be false when 'filename' is not a string\./;
assert.throws(() => new Worker(new URL(import.meta.url), { eval: true }), re);
