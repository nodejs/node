'use strict';

const common = require('../common');
const { test } = require('node:test');
const assert = require('node:assert');
const { constants } = require('node:os');

const { internalBinding } = require('internal/test/binding');

test('Verify that signals constant is immutable', () => {
    assert.throws(() => constants.signals.FOOBAR = 1337, TypeError);
})