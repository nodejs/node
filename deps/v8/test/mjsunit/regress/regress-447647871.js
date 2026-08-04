// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(['0xx0x'], /0xx0x|0xcat/i.exec('0xx0xfoobar'));
assertEquals(['0xcat'], /0xx0x|0xcat/i.exec('0xcatfoobar'));
