const assert = require('assert');
const test = require('node:test');

test('keep error', (t) => {
    assert.strictEqual(0, 1);
});

test('dont show', t => {
    assert.strictEqual(0, 2);
})
