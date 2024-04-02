'use strict';
require('../../../common');
const { test } = require('node:test');

// Do not include any failing tests in this file.

// A test whose description needs to be escaped.
test('escaped description \\ # \\#\\ \n \t \f \v \b \r');

// A test whose skip message needs to be escaped.
test('escaped skip message', { skip: '#skip' });

// A test whose todo message needs to be escaped.
test('escaped todo message', { todo: '#todo' });

// A test with a diagnostic message that needs to be escaped.
test('escaped diagnostic', (t) => {
  t.diagnostic('#diagnostic');
});
