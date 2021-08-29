// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testLoad = 'test_load';
d8.file.execute('test_data/mjsunit/test_load_1.js');
d8.file.execute('test_load_0.js');
