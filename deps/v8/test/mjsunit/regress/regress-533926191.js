// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const options = {
  get create_own_microtask_queue() {
    throw 'bonk!';
  }
};

this.Realm.create(options);
