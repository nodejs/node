// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getNS} from 'regress-v8-12729.mjs';

assertThrows(
    getNS, ReferenceError, 'Cannot access \'default\' before initialization');
export default 0;
