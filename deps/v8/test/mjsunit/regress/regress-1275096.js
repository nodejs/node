// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-regexp-tier-up

/[\u{0}zPudf\u{d3}-\ud809\udccc]/iu.exec("");  // Don't crash :)
