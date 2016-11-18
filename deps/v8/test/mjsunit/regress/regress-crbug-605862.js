// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/[]*1/u.exec("\u1234");
/[^\u0000-\u{10ffff}]*1/u.exec("\u1234");
