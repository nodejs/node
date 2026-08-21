// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('sort.js');

new BenchmarkSuite('SortFloatTypes', [1000],
                   CreateBenchmarks(typedArrayFloatConstructors));
