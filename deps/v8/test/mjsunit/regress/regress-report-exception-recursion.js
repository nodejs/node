// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --throws

// Reporting an uncaught exception stringifies it. If its toString throws, the
// conversion fails and d8's ReportException must bail instead of re-reporting
// the newly thrown exception, which would recurse into toString again.
const obj = {
  toString() {
    throw obj;
  }
};
throw obj;
