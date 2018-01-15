// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-dynamic-import

eval(`import('modules-skip-2.js');`);
eval(`eval(import('modules-skip-2.js'));`);
