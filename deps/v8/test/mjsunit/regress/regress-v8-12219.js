// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function quitInWorker() {
  quit();
};

for(let i = 0; i < 10; i++){
  new Worker(quitInWorker, ({type : 'function', arguments : []}));
}
