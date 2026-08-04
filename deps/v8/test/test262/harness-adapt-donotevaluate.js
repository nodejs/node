// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// V8 has several long-standing bugs where "early errors", i.e. errors that are
// supposed to be thrown at parse time, end up being thrown at runtime instead.
// This file is used to implement the FAIL_PHASE_ONLY outcome as used in
// test/test262/test262.status. Tests marked with this outcome are run in a
// special mode that verifies that a) V8 throws an exception at all, and b) that
// the exception has the correct type, but ignores the fact that they are thrown
// after parsing is done. See crbug.com/v8/8467 for details.
$DONOTEVALUATE = () => {};
