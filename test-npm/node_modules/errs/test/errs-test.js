/*
 * errs-test.js: Tests for the `errs` module.
 *
 * (C) 2012, Charlie Robbins, Nuno Job, and the Contributors.
 * MIT LICENSE
 *
 */

var assert = require('assert'),
    events = require('events'),
    vows = require('vows'),
    errs = require('../lib/errs'),
    fixtures = require('./fixtures'),
    macros = require('./macros');

var opts = [{
  foo: 'bar',
  status: 404,
  whatever: 'some other property'
}, {
  testing: true,
  'some-string': 'is-a-value',
  message: 'This is an error. There are many like it.'
}, {
  'a-function': 'that returns an object',
  should: true,
  have: 4,
  properties: 'yes'
}];

vows.describe('errs').addBatch({
  "Using errs module": {
    "the register() method": {
      "should register the prototype": function () {
        errs.register('named', fixtures.NamedError);
        assert.equal(errs.registered['named'], fixtures.NamedError);
      },
      "should register an error without providing its name": function () {
        errs.register(fixtures.AnError);
        assert.equal(errs.registered['anerror'], fixtures.AnError);
      }
    },
    "the create() method with": {
      "a string": macros.create.string('An error as a string'),
      "no parameters": macros.create.string('An error as a string'),
      "an object": {
        "that has no message": macros.create.object(opts[0]),
        "that has a message": macros.create.object(opts[1]),
        "that has a name": {
          topic : errs.create({name: 'OverflowError'}),
          "should respect that name in the stack trace" : function (err) {
            assert.match(err.stack, /^OverflowError/);
          },
        }
      },
      "an error": macros.create.err(new Error('An instance of an error')),
      "a function": macros.create.fn(function () {
        return opts[2];
      }),
      "a registered type": {
        "that exists": macros.create.registered('named', fixtures.NamedError, opts[1]),
        "that doesnt exist": macros.create.registered('bad', null, opts[1])
      }
    },
    "the handle() method": {
      "with a callback": {
        topic: function () {
          var err = this.err = errs.create('Some async error');
          errs.handle(err, this.callback.bind(this, null));
        },
        "should invoke the callback with the error": function (_, err) {
          assert.equal(err, this.err);
        }
      },
      "with an EventEmitter (i.e. stream)": {
        topic: function () {
          var err = this.err = errs.create('Some emitted error'),
              stream = new events.EventEmitter();

          stream.once('error', this.callback.bind(this, null));
          errs.handle(err, stream);
        },
        "should emit the `error` event": function (_, err) {
          assert.equal(err, this.err);
        }
      },
      "with a callback and an EventEmitter": {
        topic: function () {
          var err = this.err = errs.create('Some emitted error'),
              stream = new events.EventEmitter(),
              invoked = 0,
              that = this;

          function onError(err) {
            if (++invoked === 2) {
              that.callback.call(that, null, err);
            }
          }

          stream.once('error', onError);
          errs.handle(err, onError, stream);
        },
        "should emit the `error` event": function (_, err) {
          assert.equal(err, this.err);
        }
      },
      "with no callback": {
        topic: function () {
          var err = this.err = errs.create('Some emitted error'),
              emitter = errs.handle(err);

          emitter.once('error', this.callback.bind(this, null));
        },
        "should emit the `error` event": function (_, err) {
          assert.equal(err, this.err);
        }
      }
    }
  }
}).addBatch({
  "Using errs module": {
    "the unregister() method": {
      "should unregister the prototype": function () {
        errs.unregister('named');
        assert.isTrue(!errs.registered['named']);
      }
    }
  }
}).addBatch({
  "Using errs module": {
    "the merge() method": {
      "supports": {
        "an undefined error": function () {
          var err = errs.merge(undefined, { message: 'oh noes!' });
          assert.equal(err.message, 'oh noes!')
          assert.instanceOf(err, Error);
        },
        "a null error": function () {
          var err = errs.merge(null, { message: 'oh noes!' });
          assert.equal(err.message, 'oh noes!')
          assert.instanceOf(err, Error);
        },
        "a false error": function () {
          var err = errs.merge(false, { message: 'oh noes!' });
          assert.equal(err.message, 'oh noes!')
          assert.instanceOf(err, Error);
        },
        "a string error": function () {
          var err = errs.merge('wat', { message: 'oh noes!' });
          assert.equal(err.message, 'oh noes!');
          assert.instanceOf(err, Error);
        },
      },
      "should preserve custom properties": function () {
        var err = new Error('Msg!');
        err.foo = "bar";
        err = errs.merge(err, {message: "Override!", ns: "test"});
        assert.equal(err.foo, "bar");
      },
      "should have a stack trace": function () {
        var err = new Error('Msg!');
        err = errs.merge(err, {});
        assert.isTrue(Array.isArray(err.stacktrace));
      },
      "should preserve message specified in create": function () {
        var err = new Error('Msg!');
        err = errs.merge(err, {message: "Override!"});
        assert.equal(err.message, "Override!");
      },
      "should preserve properties specified": function () {
        var err = new Error('Msg!');
        err = errs.merge(err, {ns: "test"});
        assert.equal(err.ns, "test");
      },
      "with a truthy value": function () {
        var err = errs.merge(true, {
          message: 'Override!',
          ns: 'lolwut'
        })
        assert.equal(err.message, 'Override!');
        assert.equal(err.ns, 'lolwut');
      },
      "with a truthy stack": function () {
        var err = errs.merge({ stack: true } , {
          message: 'Override!',
          ns: 'lolwut'
        })
        assert.equal(err.message, 'Override!');
        assert.equal(err.ns, 'lolwut');
      },
      "with an Array stack": function () {
        var err = errs.merge({ stack: [] } , {
          message: 'Override!',
          ns: 'lolwut'
        })
        assert.equal(err.message, 'Override!');
        assert.equal(err.ns, 'lolwut');
      }
    }
  }
}).addBatch({
  "Using errs module": {
    "Error.prototype.toJSON": {
      "should exist": function () {
        assert.isFunction(Error.prototype.toJSON);

        var json = (new Error('Testing 12345')).toJSON();

        ['message', 'stack', 'arguments', 'type'].forEach(function (prop) {
          assert.isObject(Object.getOwnPropertyDescriptor(json, prop));
        })
      },
      "should be writable": function () {
        var orig = Error.prototype.toJSON;
        Error.prototype.toJSON = function() {
          return 'foo';
        };
        var json = (new Error('Testing 12345')).toJSON();

        assert.equal(json, 'foo');
        Error.prototype.toJSON = orig;
      }
    }
  }
}).export(module);
