// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[
  "--first-config=ignition",
  "--second-config=ignition_turbo",
  "--second-d8=d8",
  "--second-config-extra-flags=--foo1",
  "--second-config-extra-flags=--foo2",
  "--first-config-extra-flags=--fuzzilli-flag1",
  "--second-config-extra-flags=--fuzzilli-flag1",
  "--first-config-extra-flags=--fuzzilli-flag2",
  "--second-config-extra-flags=--fuzzilli-flag2"
]
