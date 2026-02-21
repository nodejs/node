// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This neuters too low stack size passed by the flag fuzzer.
// Flags: --stack-size=1000

var nativeJSON = this.JSON;
this.JSON = null;
d8.file.execute("test/webkit/resources/json2-es5-compat.js");
d8.file.execute("test/webkit/resources/JSON-stringify.js");
