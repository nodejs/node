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
const events = require('events');
const e = new events.EventEmitter();

e.on('maxListeners', common.mustCall());

// Should not corrupt the 'maxListeners' queue.
e.setMaxListeners(42);

const throwsObjs = [NaN, -1, 'and even this'];

for (const obj of throwsObjs) {
  common.expectsError(
    () => e.setMaxListeners(obj),
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "n" is out of range. ' +
               `It must be a non-negative number. Received ${obj}`
    }
  );

  common.expectsError(
    () => events.defaultMaxListeners = obj,
    {
      code: 'ERR_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "defaultMaxListeners" is out of range. ' +
               `It must be a non-negative number. Received ${obj}`
    }
  );
}

e.emit('maxListeners');
