'use strict';

require('../common');
const assert = require('assert');
const domain = require('domain');

const d = new domain.Domain();

assert.strictEqual(d.run(() => 'return value'),
                   'return value');

assert.strictEqual(d.run((a, b) => `${a} ${b}`, 'return', 'value'),
                   'return value');
