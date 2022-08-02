// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The WebAssembly spec doesn't specify a limit on how many Memory objects can
// be allocated. This test makes sure we can have at least two at once.

importScripts("/resources/testharness.js");
test(function () {
  const mem1 = new WebAssembly.Memory({initial: 1});
  const mem2 = new WebAssembly.Memory({initial: 1});
}, "WebAssembly#CreateMultipleMemories.");
done();
