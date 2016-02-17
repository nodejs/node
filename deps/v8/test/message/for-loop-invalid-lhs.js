// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(adamk): Remove flag after the test runner tests all message tests with
// the preparser: https://code.google.com/p/v8/issues/detail?id=4372
// Flags: --min-preparse-length=0

function f() { for ("unassignable" in {}); }
