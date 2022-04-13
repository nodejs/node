// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import("./modules-skip-reset1.js").then(
    () => assertUnreachable("must throw"), () => {});
import("./modules-skip-reset3.js").then(
    () => assertUnreachable("must throw"), () => {});
