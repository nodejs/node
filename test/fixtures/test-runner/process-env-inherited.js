const { test } = require('node:test');

test('process.env is inherited when no env option is provided', (t) => {
    t.assert.strictEqual(process.env.INHERITED_VAR, 'XYZ', 'parent env var should be inherited by default');
    t.assert.strictEqual(process.env.NODE_TEST_CONTEXT, 'child-v8', 'NODE_TEST_CONTEXT should be set by run()');
});
