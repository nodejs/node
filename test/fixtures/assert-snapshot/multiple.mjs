import assert from 'node:assert';

await assert.snapshot('test', 'name');
await assert.snapshot('test', 'another name');
