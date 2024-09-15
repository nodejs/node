const { describe, test } = require('node:test');
const assert = require('node:assert');

describe('Suite', () => {
    test('Failing test', () => {
        assert.fail()
    })

    test('Passing test', () => {
        assert.ok(true)
    })
});
