'use strict';
/* eslint-disable max-len */

require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

/* For the moment we are not going to support setting the timezone via the
 * environment variables. The problem is that various V8 platform backends
 * deal with timezone in different ways. The windows platform backend caches
 * the timezone value while the Linux one hits libc for every query.

https://github.com/joyent/node/blob/08782931205bc4f6d28102ebc29fd806e8ccdf1f/deps/v8/src/platform-linux.cc#L339-345
https://github.com/joyent/node/blob/08782931205bc4f6d28102ebc29fd806e8ccdf1f/deps/v8/src/platform-win32.cc#L590-596

// first things first, set the timezone; see tzset(3)
process.env.TZ = 'Europe/Amsterdam';

// time difference between Greenwich and Amsterdam is +2 hours in the summer
date = new Date('Fri, 10 Sep 1982 03:15:00 GMT');
assert.equal(3, date.getUTCHours());
assert.equal(5, date.getHours());
*/


// changes in environment should be visible to child processes
if (process.argv[2] == 'you-are-the-child') {
  // failed assertion results in process exiting with status code 1
  assert.strictEqual(false, 'NODE_PROCESS_ENV_DELETED' in process.env);
  assert.strictEqual('42', process.env.NODE_PROCESS_ENV);
  assert.strictEqual('asdf', process.env.hasOwnProperty);
  var hasOwnProperty = Object.prototype.hasOwnProperty;
  const has = hasOwnProperty.call(process.env, 'hasOwnProperty');
  assert.strictEqual(true, has);
  process.exit(0);
} else {
  assert.strictEqual(Object.prototype.hasOwnProperty, process.env.hasOwnProperty);
  const has = process.env.hasOwnProperty('hasOwnProperty');
  assert.strictEqual(false, has);

  process.env.hasOwnProperty = 'asdf';

  process.env.NODE_PROCESS_ENV = 42;
  assert.strictEqual('42', process.env.NODE_PROCESS_ENV);

  process.env.NODE_PROCESS_ENV_DELETED = 42;
  assert.strictEqual(true, 'NODE_PROCESS_ENV_DELETED' in process.env);

  delete process.env.NODE_PROCESS_ENV_DELETED;
  assert.strictEqual(false, 'NODE_PROCESS_ENV_DELETED' in process.env);

  var child = spawn(process.argv[0], [process.argv[1], 'you-are-the-child']);
  child.stdout.on('data', function(data) { console.log(data.toString()); });
  child.stderr.on('data', function(data) { console.log(data.toString()); });
  child.on('exit', function(statusCode) {
    if (statusCode != 0) {
      process.exit(statusCode);  // failed assertion in child process
    }
  });
}

// delete should return true except for non-configurable properties
// https://github.com/nodejs/node/issues/7960
delete process.env.NON_EXISTING_VARIABLE;
assert.strictEqual(true, delete process.env.NON_EXISTING_VARIABLE);
