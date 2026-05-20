// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --interrupt-budget=1024

async function* r() {
  for (var l = "" in { goo: ()=>{} }) {
    for (let n = 0; n < 500; (t ? -500 : 0)) {
      n++;
      if (n > 1) break;
      try {
        r.blabadfasdfasdfsdafsdsadf();
      } catch (e) {
        for (let n = 0; n < 500; n++);
        for (let n in t) {
          return t[n];
        }
      }
      try { r(n, null) } catch (e) {}
    }
  }
}
let t = r();
t.return({
  get then() {
    let n = r();
    n.next();
  }
});
