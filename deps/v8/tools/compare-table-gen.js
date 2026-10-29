// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generates a comparison table test case.
// Usage: d8 compare-table-gen.js -- lt|lteq|gt|gteq|eq|ne|eq|sne|min|max

var strings = ["true", "false", "null", "void 0", "0", "0.0", "-0", "\"\"", "-1", "-1.25", "1", "1.25", "-2147483648", "2147483648", "Infinity", "-Infinity", "NaN"];
var values = new Array(strings.length);
for (var i = 0; i < strings.length; i++) {
  values[i] = eval(strings[i]);
}

function test() {
  for (var i = 0; i < values.length; i++) {
    for (var j = 0; j < values.length; j++) {
      var a = values[i];
      var b = values[j];
      var x = expected[i][j];
      assertEquals(x, func(a,b));
      assertEquals(x, left_funcs[i](b));
      assertEquals(x, right_funcs[j](a));
    }
  }

  var result = matrix();
  for (var i = 0; i < values.length; i++) {
    for (var j = 0; j < values.length; j++) {
      assertEquals(expected[i][j], result[i][j]);
    }
  }
}

function expr(infix, a, cmp, b) {
  return infix ? a + " " + cmp + " " + b : cmp + "(" + a + ", " + b + ")";
}

function SpecialToString(x) {
  if ((1 / x) == -Infinity) return "-0";
  return "" + x;
}

function gen(name, cmp, infix) {

  print("// Copyright 2015 the V8 project authors. All rights reserved.");
  print("// Use of this source code is governed by a BSD-style license that can be");
  print("// found in the LICENSE file.");
  print();
  print("var values = [" + strings + "];");

  var body = "(function " + name + "(a,b) { return " + expr(infix, "a", cmp, "b") + "; })";
  var func = eval(body);

  print("var expected = [");

  for (var i = 0; i < values.length; i++) {
    var line = "  [";
    for (var j = 0; j < values.length; j++) {
      if (j > 0) line += ",";
      line += SpecialToString(func(values[i], values[j]));
    }
    line += "]";
    if (i < (values.length - 1)) line += ",";
    print(line);
  }
  print("];");

  print("var func = " + body + ";");
  print("var left_funcs = [");

  for (var i = 0; i < values.length; i++) {
    var value = strings[i];
    var body = "(function " + name + "_L" + i + "(b) { return " + expr(infix, value, cmp, "b") + "; })";
    var end = i < (values.length - 1) ? "," : "";
    print("  " + body + end);
  }
  print("];");

  print("var right_funcs = [");
  for (var i = 0; i < values.length; i++) {
    var value = strings[i];
    var body = "(function " + name + "_R" + i + "(a) { return " + expr(infix, "a", cmp, value) + "; })";
    var end = i < (values.length - 1) ? "," : "";
    print("  " + body + end);
  }
  print("];");

  print("function matrix() {");
  print("  return [");
  for (var i = 0; i < values.length; i++) {
    var line = "    [";
    for (var j = 0; j < values.length; j++) {
      if (j > 0) line += ",";
      line += expr(infix, strings[i], cmp, strings[j]);
    }
    line += "]";
    if (i < (values.length - 1)) line += ",";
    print(line);
  }
  print("  ];");
  print("}");


  print(test.toString());
  print("test();");
  print("test();");
}

switch (arguments[0]) {
  case "lt":   gen("lt",   "<", true); break;
  case "lteq": gen("lteq", "<=", true); break;
  case "gt":   gen("gt",   ">", true); break;
  case "gteq": gen("gteq", ">=", true); break;
  case "eq":   gen("eq",   "==", true); break;
  case "ne":   gen("ne",   "!=", true); break;
  case "seq":  gen("seq",  "===", true); break;
  case "sne":  gen("sne",  "!==", true); break;
  case "min":  gen("min",  "Math.min", false); break;
  case "max":  gen("max",  "Math.max", false); break;
}
