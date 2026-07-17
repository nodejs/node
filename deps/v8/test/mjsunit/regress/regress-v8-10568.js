// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertEquals(/[--𐀾]/u.exec("Hr3QoS3KCWXQ2yjBoDIK")[0], "H");
assertEquals(/[0-\u{10000}]/u.exec("A0")[0], "A");
