// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --debug-code

// flag --debug-code ensures that we'll abort with a failed smi check without
// the fix.

var num = new Number(10);
Array.prototype.__defineGetter__(0,function(){
    return num;
})
Array.prototype.__defineSetter__(0,function(value){
})
var str=decodeURI("%E7%9A%84");
assertEquals(0x7684, str.charCodeAt(0));
