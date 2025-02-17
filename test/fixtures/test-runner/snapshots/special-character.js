'use strict';
const { test } = require('node:test');

test('\r', (t) => {
  t.assert.snapshot({ key: 'value' });
});

test(String.fromCharCode(55296), t => {
	t.assert.snapshot({key: 'value'});
});

test(String.fromCharCode(57343), t => {
	t.assert.snapshot({key: 'value'});
});
