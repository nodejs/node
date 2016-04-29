// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noharmony-iterator-close

// The {Set} function will produce a different type feedback vector layout
// depending on whether Harmony iterator finalization is enabled or not.

new Set();
