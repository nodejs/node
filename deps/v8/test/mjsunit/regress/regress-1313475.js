// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc --invoke-weak-callbacks

// We spawn a new worker which creates a Realm, then terminate the main thread
// which will also terminate the worker.
new Worker(`Realm.create();`, {type: 'string'});
