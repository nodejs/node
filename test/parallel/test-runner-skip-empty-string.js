'use strict';

const common = require('../common');
const { test } = require('node:test');

test('skip option empty string should not skip', { skip: '' }, common.mustCall());
