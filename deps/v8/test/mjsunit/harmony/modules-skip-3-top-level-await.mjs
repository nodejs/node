// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as m1 from 'modules-skip-1-top-level-await.mjs';
import * as m2 from 'modules-skip-3.mjs';

export function life() {
  return m1.life();
}

export let stringlife = m2.stringlife;
