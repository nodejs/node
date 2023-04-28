'use strict';

require('../common');

const assert = require('assert');
const Transform = require('stream').Transform;


const expected = 'asdf';


function _transform(d, e, n) {
  n();
}

function _flush(n) {
  n(null, expected);
}

const t = new Transform({
  transform: _transform,
  flush: _flush
});

t.end(Buffer.from('blerg'));
t.on('data', (data) => {
  assert.strictEqual(data.toString(), expected);
});
