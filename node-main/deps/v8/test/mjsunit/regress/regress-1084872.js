// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = new RegExp("(?<=(.)\\2.*(T))");
re.exec(undefined);
assertEquals(re.exec("bTaLTT")[1], "b");
