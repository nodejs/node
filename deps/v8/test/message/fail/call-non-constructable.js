// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note that {unescape} is an example of a non-constructable function. If that
// ever changes and this test needs to be adapted, make sure to choose another
// non-constructable {JSFunction} object instead.
new unescape();
