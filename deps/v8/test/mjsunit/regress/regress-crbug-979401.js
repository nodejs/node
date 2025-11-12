// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let min_fields = 1015;
let max_fields = 1025;

let static_fields_src = "";
let instance_fields_src = "";
for (let i = 0; i < max_fields; i++) {
  static_fields_src += "  static f" + i + "() {}\n";
  instance_fields_src += "  g" + i + "() {}\n";

  if (i >= min_fields) {
    let src1 = "class A {\n" + static_fields_src + "}\n";
    eval(src1);
    let src2 = "class B {\n" + instance_fields_src + "}\n";
    eval(src2);
  }
}
