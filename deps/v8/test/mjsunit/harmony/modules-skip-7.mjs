// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

export async function getLife() {
  try {
    let namespace = await import('modules-skip-1.mjs');
    return namespace.life();
  } catch (e) {
    %AbortJS('failure: ' + e);
  }
}
