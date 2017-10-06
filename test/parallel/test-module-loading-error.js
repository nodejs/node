// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const { execSync } = require('child_process');

const errorMessagesByPlatform = {
  win32: ['is not a valid Win32 application'],
  linux: ['file too short', 'Exec format error'],
  sunos: ['unknown file type', 'not an ELF file'],
  darwin: ['file too short'],
  aix: ['Cannot load module',
        'Cannot run a file that does not have a valid format.']
};
// If we don't know a priori what the error would be, we accept anything.
const errorMessages = errorMessagesByPlatform[process.platform] || [''];

// On Windows, error messages are MUI dependent
// Ref: https://github.com/nodejs/node/issues/13376
let localeOk = true;
if (common.isWindows) {
  const powerShellFindMUI =
    'powershell -NoProfile -ExecutionPolicy Unrestricted -c ' +
    '"(Get-UICulture).TwoLetterISOLanguageName"';
  try {
    // If MUI != 'en' we'll ignore the content of the message
    localeOk = execSync(powerShellFindMUI).toString('utf8').trim() === 'en';
  } catch (_) {
    // It's only a best effort try to find the MUI
  }
}

assert.throws(
  () => { require('../fixtures/module-loading-error.node'); },
  (e) => {
    if (localeOk && !errorMessages.some((msg) => e.message.includes(msg)))
      return false;
    return e.name === 'Error';
  }
);

assert.throws(
  require,
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: /^missing path$/
  }));

assert.throws(
  () => { require({}); },
  common.expectsError({
    code: 'ERR_ASSERTION',
    message: /^path must be a string$/
  }));
