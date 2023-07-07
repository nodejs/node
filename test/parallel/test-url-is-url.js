// Flags: --expose-internals
'use strict';

require('../common');

const { URL, parse } = require('url');
const assert = require('assert');
const { isURL } = require('internal/url');

assert.strictEqual(isURL(new URL('https://www.nodejs.org')), true);
assert.strictEqual(isURL(parse('https://www.nodejs.org')), false);
