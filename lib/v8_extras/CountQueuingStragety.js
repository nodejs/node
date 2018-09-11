// Copyright 2015 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// eslint-disable-next-line no-unused-vars
(function(global, binding, v8) {
  'use strict';

  const defineProperty = global.Object.defineProperty;

  class CountQueuingStrategy {
    constructor(options) {
      defineProperty(this, 'highWaterMark', {
        value: options.highWaterMark,
        enumerable: true,
        configurable: true,
        writable: true
      });
    }

    size() {
      return 1;
    }
  }

  defineProperty(global, 'CountQueuingStrategy', {
    value: CountQueuingStrategy,
    enumerable: false,
    configurable: true,
    writable: true
  });

  // Export a separate copy that doesn't need options objects and can't be
  // interfered with.
  class BuiltInCountQueuingStrategy {
    constructor(highWaterMark) {
      defineProperty(this, 'highWaterMark', { value: highWaterMark });
    }

    size() {
      return 1;
    }
  }

  binding.createBuiltInCountQueuingStrategy = (highWaterMark) =>
    new BuiltInCountQueuingStrategy(highWaterMark);
});
