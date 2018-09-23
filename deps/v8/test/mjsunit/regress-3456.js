// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length 1

// Arrow function parsing (commit r22366) changed the flags stored in
// PreParserExpression, and IsValidReferenceExpression() would return
// false for certain valid expressions. This case is the minimum amount
// of code needed to validate that IsValidReferenceExpression() works
// properly. If it does not, a ReferenceError is thrown during parsing.

function f() { ++(this.foo) }
