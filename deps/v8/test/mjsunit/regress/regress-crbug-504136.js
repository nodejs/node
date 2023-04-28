// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (this.Worker) {
  var __v_10 = new Worker('', {type: 'string'});
  __v_10.terminate();
 __v_10.getMessage();
}
