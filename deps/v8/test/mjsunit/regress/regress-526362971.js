// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const { getExtrasBindingObject } = d8;
const {
  getContinuationPreservedEmbedderData: getCPED,
  setContinuationPreservedEmbedderData: setCPED
} = getExtrasBindingObject();

const S = { id: 'SENTINEL' };
let p1 = 'NOT_RUN', p2 = 'NOT_RUN', p3 = 'NOT_RUN', p4 = 'NOT_RUN';

setCPED(S);
%EnqueueMicrotask(() => {
  throw {
    toString() {
      p2 = getCPED();
      %EnqueueMicrotask(() => {
        p3 = getCPED();
        %EnqueueMicrotask(() => { p4 = getCPED(); });
      });
      return 'x';
    }
  };
});
setCPED(undefined);
%EnqueueMicrotask(() => { p1 = getCPED(); });

%PerformMicrotaskCheckpoint();
%PerformMicrotaskCheckpoint();
%PerformMicrotaskCheckpoint();

assertEquals(undefined, p1);
assertEquals(undefined, p2);
assertEquals(undefined, p3);
assertEquals(undefined, p4);
