'use strict';
const common = require('../common');

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

common.throws(doSetTimeout('foo'),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetTimeout({foo: 'bar'}),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetTimeout(),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetTimeout(undefined, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetTimeout(null, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetTimeout(false, 0),
  {code: 'CALLBACKREQUIRED'});


function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

common.throws(doSetInterval('foo'),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetInterval({foo: 'bar'}),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetInterval(),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetInterval(undefined, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetInterval(null, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetInterval(false, 0),
  {code: 'CALLBACKREQUIRED'});


function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

common.throws(doSetImmediate('foo'),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetImmediate({foo: 'bar'}),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetImmediate(),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetImmediate(undefined, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetImmediate(null, 0),
  {code: 'CALLBACKREQUIRED'});
common.throws(doSetImmediate(false, 0),
  {code: 'CALLBACKREQUIRED'});
