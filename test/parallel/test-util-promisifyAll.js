'use strict';
// Flags: --expose-internals
const common = require('../common');
const assert = require('assert');
const { promisifyAll } = require('util');

common.crashOnUnhandledRejection();

{

  const pobj = promisifyAll({
    string: '',
    int: 2,
    func: function(cb) {
      cb(null);
    },
    obj: {
      func: function(cb) {
        cb(null);
      },
      string: '',
      int: 2
    }
  });

  // Does not promisfy non functions
  assert.strictEqual(typeof pobj.string, 'string');
  assert.strictEqual(typeof pobj.int, 'number');
  assert.strictEqual(typeof pobj.obj, 'object');
  assert.strictEqual(typeof pobj.func, 'function');
  assert(pobj.func() instanceof Promise);
  assert.strictEqual(typeof pobj.obj.func, 'function');
  assert(pobj.obj.func() instanceof Promise);
  assert.strictEqual(typeof pobj.obj.string, 'string');
  assert.strictEqual(typeof pobj.obj.int, 'number');
}
