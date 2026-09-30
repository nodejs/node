// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strict-termination-checks

// Reporting an uncaught exception stringifies it, which can run a user
// toString that terminates execution. d8's ReportException must bail before
// re-entering V8 for the remaining message conversions once terminating,
// otherwise the strict terminating DCHECK in EnterV8BasicScope trips.
throw {
  toString() {
    d8.terminate();
    while (true) {}
  }
};
