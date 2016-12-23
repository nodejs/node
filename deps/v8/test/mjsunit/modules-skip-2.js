// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export {a as b, default} from "modules-skip-1.js";
import {a as tmp} from "modules-skip-1.js";
export {tmp as c};
