// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  let resolve;
  let onFulfilledValue;
  const p = new Promise(r => resolve = r);
  resolve(Promise.resolve(1));
  p.then(
      v => {
        onFulfilledValue = v;
      },
      e => {
        assertUnreachable();
      });
  setTimeout(_ => assertEquals(1, onFulfilledValue));
})();

(function() {
  let resolve;
  let onRejectedReason;
  const p = new Promise(r => resolve = r);
  resolve(Promise.reject(1));
  p.then(
      v => {
        assertUnreachable();
      },
      e => {
        onRejectedReason = e;
      });
  setTimeout(_ => assertEquals(1, onRejectedReason));
})();

(function() {
  let onFulfilledValue;
  (async () => Promise.resolve(1))().then(
      v => {
        onFulfilledValue = v;
      },
      e => {
        assertUnreachable();
      });
  setTimeout(_ => assertEquals(1, onFulfilledValue));
})();

(function() {
  let onRejectedReason;
  (async () => Promise.reject(1))().then(
      v => {
        assertUnreachable();
      },
      e => {
        onRejectedReason = e;
      });
  setTimeout(_ => assertEquals(1, onRejectedReason));
})();

(function() {
  let resolve;
  let onFulfilledValue;
  const p = new Promise(r => resolve = r);
  resolve({
    then(onFulfilled, onRejected) {
      onFulfilled(1);
    }
  });
  p.then(
      v => {
        onFulfilledValue = v;
      },
      e => {
        assertUnreachable();
      });
  setTimeout(_ => assertEquals(1, onFulfilledValue));
})();

(function() {
  let resolve;
  let onRejectedReason;
  const p = new Promise(r => resolve = r);
  resolve({
    then(onFulfilled, onRejected) {
      onRejected(1);
    }
  });
  p.then(
      v => {
        assertUnreachable();
      },
      e => {
        onRejectedReason = e;
      });
  setTimeout(_ => assertEquals(1, onRejectedReason));
})();

(function() {
  let onFulfilledValue;
  (async () => ({
    then(onFulfilled, onRejected) {
      onFulfilled(1);
    }
  }))().then(
      v => {
        onFulfilledValue = v;
      },
      e => {
        assertUnreachable();
      });
  setTimeout(_ => assertEquals(1, onFulfilledValue));
})();

(function() {
  let onRejectedReason;
  (async () => ({
    then(onFulfilled, onRejected) {
      onRejected(1);
    }
  }))().then(
      v => {
        assertUnreachable();
      },
      e => {
        onRejectedReason = e;
      });
  setTimeout(_ => assertEquals(1, onRejectedReason));
})();
