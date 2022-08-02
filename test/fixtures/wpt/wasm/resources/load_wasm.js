// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createWasmModule() {
    return fetch('/wasm/incrementer.wasm')
        .then(response => {
            if (!response.ok) throw new Error(response.statusText);
            return response.arrayBuffer();
        })
        .then(WebAssembly.compile);
}
