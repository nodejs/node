//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

const size = 100;
function Foo() {
  this.a = new Array(size);
  this.b = new Array(size);
  this.c = new Array(size);
  this.d = new Array(size);
  this.e = new Array(size);
  this.a.fill(1);
  this.b.fill(1);
  this.c.fill(1);
  this.d.fill(1);
  this.e.fill(1);

  this.fieldMemop = function() {
    let al = this.a.length;
    this.total = 0;
    // Right now this is invalid
    for(let i = 0; i < al; ++i) {
      this.a[i] = this.b[i];
      this.e[i] = 0;
    }
    this.validFieldMemop();
    this.validObjFieldMemop({c: this.c, d: this.d});
    this.invalidObjFieldMemop({c: this.c, d: this.d});
  };

  this.validFieldMemop = function() {
    let cl = this.c.length;
    this.total = 0;
    let c = this.c, d = this.d;
    // This is valid
    for(let i = 0; i < cl; ++i) {
      c[i] = d[i];
    }
  };

  this.validObjFieldMemop = function(obj) {
    let cl = obj.c.length;
    let c = obj.c, d = obj.d;
    // This is valid
    for(let i = 0; i < cl; ++i) {
      c[i] = d[i];
    }
  };

  this.invalidObjFieldMemop = function(obj) {
    let cl = obj.c.length;
    obj.total = 0;
    // Right now this is invalid
    for(let i = 0; i < cl; ++i) {
      obj.c[i] = obj.d[i];
      obj.total = 1;
    }
  };
}
const f = new Foo();
f.fieldMemop();
f.fieldMemop();
print("PASSED");
