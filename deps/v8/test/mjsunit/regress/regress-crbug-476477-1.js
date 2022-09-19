// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var obj = {
  _leftTime: 12345678,
  _divider: function() {
      var s = Math.floor(this._leftTime / 3600);
      var e = Math.floor(s / 24);
      var i = s % 24;
      return {
            s: s,
            e: e,
            i: i,
          }
    }
}

for (var i = 0; i < 1000; i++) {
  obj._divider();
}
