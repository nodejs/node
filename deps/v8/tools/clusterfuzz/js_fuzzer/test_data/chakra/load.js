// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.WScript && this.WScript.LoadScriptFile) {
  var { script } = WScript.LoadScriptFile("load1.js");
}

var { script1 } = WScript.LoadScriptFile("load1.js"),
    { script2 } = WScript.LoadScriptFile("load1.js");

console.log('load.js');

script();
