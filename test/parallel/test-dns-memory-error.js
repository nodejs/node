'use strict';

// Check that if libuv reports a memory error on a DNS query, that the memory
// error is passed through and not replaced with ENOTFOUND.

const common = require('../common');

common.requireFlags(['--expose-internals']);

const assert = require('assert');
const errors = require('internal/errors');
const { internalBinding } = require('internal/test/binding');

const { UV_EAI_MEMORY } = internalBinding('uv');
const memoryError = errors.dnsException(UV_EAI_MEMORY, 'fhqwhgads');

assert.strictEqual(memoryError.code, 'EAI_MEMORY');
