// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --logfile='+' --log

const log = d8.log.getAndStop();
gc();
function __f_6() {
}
__v_1 = __f_6();
