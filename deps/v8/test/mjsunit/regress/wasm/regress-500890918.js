// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --isolate --stress-runs 2 --throws

// This test checks that d8 doesn't crash on teardown if an isolate has an
// unhandled promise rejection and --stress-runs is used.

Promise.reject();
