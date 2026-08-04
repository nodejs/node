const { test } = require('node:test');

test('process.env is correct', (t) => {
    t.assert.strictEqual(process.env.ABC, undefined, 'main process env var should be undefined');
    t.assert.strictEqual(process.env.NODE_TEST_CONTEXT, 'child-v8', 'NODE_TEST_CONTEXT should be set by run()');
    t.assert.strictEqual(process.env.FOOBAR, 'FUZZBUZZ', 'specified env var should be defined');
});
