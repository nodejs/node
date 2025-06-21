// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Array.prototype.__defineSetter__(0,function(value){});

if (this.Intl) {
  var o = new Intl.DateTimeFormat('en-US', {'timeZone': 'Asia/Katmandu'})
}
