// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is testing a leak failure.

try {
  Realm.navigate(0);
} catch(e) {}
