// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as m1 from 'modules-skip-3.mjs'

let m2 = import('modules-skip-1-top-level-await.mjs');
let m2_namespace = await m2;

export let stringlife = m1.stringlife;

export function life() {
  return m2_namespace.life();
}
