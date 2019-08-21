// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

const TEST_ITERATIONS = 1000;
const SLOW_TEST_ITERATIONS = 50;
const BITS_CASES = [32, 64, 128, 256, 512, 1024, 2048, 4096, 8192];
const RANDOM_BIGINTS_MAX_BITS = 64 * 100;
