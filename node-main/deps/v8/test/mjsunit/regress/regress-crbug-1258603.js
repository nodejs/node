// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --fuzzing --no-testing-d8-test-runner

[1,2,3].reduceRight(() => { %OptimizeOsr(1); });
