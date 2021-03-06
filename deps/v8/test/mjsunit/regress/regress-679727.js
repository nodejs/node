// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

f = (e=({} = {} = 1)) => {}; f(1);
((e = ({} = {} = {})) => {})(1)
