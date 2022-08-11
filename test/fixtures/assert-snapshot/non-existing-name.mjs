import assert from 'node:assert';

await assert.snapshot("test", "another name");
await assert.snapshot("test", "non existing");
