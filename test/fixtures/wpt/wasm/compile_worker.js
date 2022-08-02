// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.importScripts("resources/load_wasm.js");

onmessage = function(msg) {
    createWasmModule()
        .then(postMessage)

}
