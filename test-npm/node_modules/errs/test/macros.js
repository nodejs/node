/*
 * macros.js: Test macros for the `errs` module.
 *
 * (C) 2012, Charlie Robbins, Nuno Job, and the Contributors.
 * MIT LICENSE
 *
 */

var assert = require('assert'),
    errs = require('../lib/errs');

var macros = exports;

function assertTransparentStack(err) {
  assert.isString(err.stack);
  err.stack.split('\n').forEach(function (line) {
    assert.isFalse(/\/lib\/errs\.js\:/.test(line));
  });
}

//
// Macros for `errs.create(type, opts)`.
//
macros.create = {};

macros.create.string = function (msg) {
  return {
    topic: errs.create(msg),
    "should create an error with the correct message": function (err) {
      assert.instanceOf(err, Error);
      assert.equal(msg, err.message);
      assertTransparentStack(err);
    }
  };
};

macros.create.object = function (obj) {
  return {
    topic: errs.create(obj),
    "should create an error with the specified properties": function (err) {
      assert.instanceOf(err, Error);
      assert.equal(err.message, obj.message || 'Unspecified error');
      assertTransparentStack(err);
      Object.keys(obj).forEach(function (key) {
        assert.equal(err[key], obj[key]);
      });
    }
  };
};

macros.create.err = function (inst) {
  return {
    topic: errs.create(inst),
    "should return the error unmodified": function (err) {
      assert.equal(err, inst);
      assertTransparentStack(err);
    }
  };
};

macros.create.fn = function (fn) {
  var obj = fn();

  return {
    topic: errs.create(fn),
    "should create an error with the specified properties": function (err) {
      assert.instanceOf(err, Error);
      assert.equal(err.message, obj.message || 'Unspecified error');
      assertTransparentStack(err);
      Object.keys(obj).forEach(function (key) {
        assert.equal(err[key], obj[key]);
      });
    }
  };
};

macros.create.registered = function (type, proto, obj) {
  return {
    topic: function () {
      return errs.create(type, obj);
    },
    "should create an error of the correct type": function (err) {
      assert.instanceOf(err, proto || Error);
      assert.equal(err.message, obj.message || 'Unspecified error');
      assertTransparentStack(err);
      Object.keys(obj).forEach(function (key) {
        assert.equal(err[key], obj[key]);
      });
    }
  };
};