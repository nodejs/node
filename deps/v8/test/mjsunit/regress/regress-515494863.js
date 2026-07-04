// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let child = 'data:text/javascript,await Promise.resolve();';
let bad_dep = 'data:text/javascript,throw new Error("sync error");';

// parent2 imports child first, bad_dep second
let parent2 = `data:text/javascript,import '${child}'; import '${bad_dep}';`;

// parent1 imports child
let parent1 = `data:text/javascript,import '${child}';`;

import(child);
import(parent2).catch(() => {});
import(parent1);
