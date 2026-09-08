// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

const base = 'aaaaaaaabbbbbbbbbbbbbb';
const ext = createExternalizableTwoByteString(base);
const slice = /b+/.exec(ext)[0];
const o = {[ext]: 1};
assertEquals('\\x62bbbbbbbbbbbbb', RegExp.escape(slice));
