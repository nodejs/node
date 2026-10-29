// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sleeping_promise() {
  return new Promise((resolve) => setTimeout(resolve));
}

export let life;

await sleeping_promise();
life = -1;
await sleeping_promise();
life = (await import('modules-skip-1.mjs')).life();
