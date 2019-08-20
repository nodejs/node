// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import "modules-skip-empty-import.mjs";
import {} from "modules-skip-empty-import.mjs";
export {} from "modules-skip-empty-import.mjs";
import {counter} from "modules-skip-empty-import-aux.mjs";
assertEquals(1, counter);
