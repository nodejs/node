// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The following RegExp regression case from
// https://bugs.chromium.org/p/chromium/issues/detail?id=977003
let g = "[\\t\\n\\f ]";
let W = g + "*";
let h = "[\\ud800-\\udbff][\\udc00-\\udfff]";
let k = "[\\u0080-\\ud7ff\\ue000-\\ufffd]|" + h;
let U = "[0-9a-fA-F]{1,6}" + g + "?";
let E = "(?:" + U + "|[\\u0020-\\u007e\\u0080-\\ud7ff\\ue000\\ufffd]|" + h + ")";
let m = "\\\\" + E;
let o = "(?:[\\t\\x21\\x23-\\x26\\x28-\\x5b\\x5d-\\x7e]|" + k + "|" + m + ")";
let p = '[^\'"\\n\\f\\\\]|\\\\[\\s\\S]';
let q = '"(?:\'|' + p + ')*"' + '|\'(?:\"|' + p + ')*\'';
let r = "[-+]?(?:[0-9]+(?:[.][0-9]+)?|[.][0-9]+)";
let t = "(?:[a-zA-Z_]|" + k + "|" + m + ")";
let u = "(?:[a-zA-Z0-9_-]|" + k + "|" + m + ")";
let v = u + "+";
let I = "-?" + t + u + "*";
let x = "(?:@?-?" + t + "|#)" + u + "*";
let y = r + "(?:%|" + I + ")?";
let z = "url[(]" + W + "(?:" + q + "|" + o + "*)" + W + "[)]";
let B = "U[+][0-9A-F?]{1,6}(?:-[0-9A-F]{1,6})?";
let C = "<\!--";
let F = "-->";
let S = g + "+";
let G = "/(?:[*][^*]*[*]+(?:[^/][^*]*[*]+)*/|/[^\\n\\f]*)";
let J = "(?!url[(])" + I + "[(]";
let R = "[~|^$*]=";
let T = '[^"\'\\\\/]|/(?![/*])';
let V = "\\uFEFF";
let Y = [V, B, z, J, x, q, y, C, F, S, G, R, T].join("|");

function ComplexGlobalCaseInsensitiveMatch() {
  // keep the RegExp in the measurement but not string concat nor join.
  let X = new RegExp(Y, "gi");
  "abcſABCβκς".match(X);
  "color:".match(X);
}

benchmarks = [ [ComplexGlobalCaseInsensitiveMatch, () => {}],
             ];

createBenchmarkSuite("ComplexCaseInsensitiveTest");
