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

var EventEmitter = require('events');
var v8binding, process;

// This needs to be loaded early, and before the "process" object is made
// global. So allow src/node.js to pass the process object in during
// initialization.
exports._nodeInitialization = function nodeInitialization(pobj) {
  process = pobj;
  v8binding = process.binding('v8');

  // Finish setting up the v8 Object.
  v8.getHeapStatistics = v8binding.getHeapStatistics;
  v8.setFlagsFromString = v8binding.setFlagsFromString;

  // Do a little housekeeping.
  delete exports._nodeInitialization;
};


// v8

var v8 = exports.v8 = new EventEmitter();


function emitGC(before, after) {
  v8.emit('gc', before, after);
}


v8.on('newListener', function(name) {
  if (name === 'gc' && EventEmitter.listenerCount(this, name) === 0) {
    v8binding.startGarbageCollectionTracking(emitGC);
  }
});


v8.on('removeListener', function(name) {
  if (name === 'gc' && EventEmitter.listenerCount(this, name) === 0) {
    v8binding.stopGarbageCollectionTracking();
  }
});

