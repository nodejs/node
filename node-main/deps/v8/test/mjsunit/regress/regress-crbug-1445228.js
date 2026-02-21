// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AF = async function () {}.constructor;
class C extends AF {}
var f = new C("'use strict';");
