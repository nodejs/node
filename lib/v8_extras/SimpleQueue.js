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

// Queue implementation used by ReadableStream and WritableStream.

(function(global, binding, v8) {
  'use strict';

  const _front = v8.createPrivateSymbol('front');
  const _back = v8.createPrivateSymbol('back');
  const _cursor = v8.createPrivateSymbol('cursor');
  const _size = v8.createPrivateSymbol('size');
  const _elements = v8.createPrivateSymbol('elements');
  const _next = v8.createPrivateSymbol('next');

  // Take copies of global objects to protect against them being replaced.
  const RangeError = global.RangeError;

  // shift() and peek() can only be called on a non-empty queue. This function
  // throws an exception with the message mentioning |functionName| if |queue|
  // is empty.
  function requireNonEmptyQueue(queue, functionName) {
    if (queue[_size] === 0) {
      throw new RangeError(
        `${functionName}() must not be called on an empty queue`);
    }
  }

  // Simple queue structure. Avoids scalability issues with using
  // InternalPackedArray directly by using multiple arrays in a linked list and
  // keeping the array size bounded.
  const QUEUE_MAX_ARRAY_SIZE = 16384;
  class SimpleQueue {
    constructor() {
      // [_front] and [_back] are always defined.
      this[_front] = {
        [_elements]: new v8.InternalPackedArray(),
        [_next]: undefined,
      };
      this[_back] = this[_front];
      // The cursor is used to avoid calling InternalPackedArray.shift(). It
      // contains the index of the front element of the array inside the
      // frontmost node. It is always in the range [0, QUEUE_MAX_ARRAY_SIZE).
      this[_cursor] = 0;
      // When there is only one node, size === elements.length - cursor.
      this[_size] = 0;
    }

    get length() {
      return this[_size];
    }

    // For exception safety, this method is structured in order:
    // 1. Read state
    // 2. Calculate required state mutations
    // 3. Perform state mutations
    push(element) {
      const oldBack = this[_back];
      let newBack = oldBack;
      // assert(oldBack[_next] === undefined);
      if (oldBack[_elements].length === QUEUE_MAX_ARRAY_SIZE - 1) {
        newBack = {
          [_elements]: new v8.InternalPackedArray(),
          [_next]: undefined,
        };
      }

      // push() is the mutation most likely to throw an exception, so it
      // goes first.
      oldBack[_elements].push(element);
      if (newBack !== oldBack) {
        this[_back] = newBack;
        oldBack[_next] = newBack;
      }
      ++this[_size];
    }

    // Like push(), shift() follows the read -> calculate -> mutate pattern for
    // exception safety.
    shift() {
      requireNonEmptyQueue(this, 'shift');

      const oldFront = this[_front];
      let newFront = oldFront;
      const oldCursor = this[_cursor];
      let newCursor = oldCursor + 1;

      const elements = oldFront[_elements];
      const element = elements[oldCursor];

      if (newCursor === QUEUE_MAX_ARRAY_SIZE) {
        // assert(elements.length === QUEUE_MAX_ARRAY_SIZE);
        // assert(oldFront[_next] !== undefined);
        newFront = oldFront[_next];
        newCursor = 0;
      }

      // No mutations before this point.
      --this[_size];
      this[_cursor] = newCursor;
      if (oldFront !== newFront) {
        this[_front] = newFront;
      }

      // Permit shifted element to be garbage collected.
      elements[oldCursor] = undefined;

      return element;
    }

    // The tricky thing about forEach() is that it can be called
    // re-entrantly. The queue may be mutated inside the callback. It is easy to
    // see that push() within the callback has no negative effects since the end
    // of the queue is checked for on every iteration. If shift() is called
    // repeatedly within the callback then the next iteration may return an
    // element that has been removed. In this case the callback will be called
    // with undefined values until we either "catch up" with elements that still
    // exist or reach the back of the queue.
    forEach(callback) {
      let i = this[_cursor];
      let node = this[_front];
      let elements = node[_elements];
      while (i !== elements.length || node[_next] !== undefined) {
        if (i === elements.length) {
          // assert(node[_next] !== undefined);
          // assert(i === QUEUE_MAX_ARRAY_SIZE);
          node = node[_next];
          elements = node[_elements];
          i = 0;
          if (elements.length === 0) {
            break;
          }
        }
        callback(elements[i]);
        ++i;
      }
    }

    // Return the element that would be returned if shift() was called now,
    // without modifying the queue.
    peek() {
      requireNonEmptyQueue(this, 'peek');

      const front = this[_front];
      const cursor = this[_cursor];
      return front[_elements][cursor];
    }
  }

  binding.SimpleQueue = SimpleQueue;
});
