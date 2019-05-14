'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');

const message = 'message';
const testFunction = common.mustNotCall(message);

const validateError = common.mustCall((e) => {
  const prefix = `${message} at `;
  assert.ok(e.message.startsWith(prefix));
  if (process.platform === 'win32') {
    e.message = e.message.substring(2); // remove 'C:'
  }
  const [ fileName, lineNumber ] = e.message
                                    .substring(prefix.length).split(':');
  assert.strictEqual(path.basename(fileName), 'test-common-must-not-call.js');
  assert.strictEqual(lineNumber, '8');
});

try {
  testFunction();
} catch (e) {
  validateError(e);
}
