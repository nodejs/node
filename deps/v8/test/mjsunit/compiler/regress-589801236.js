// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --stress-inline

const main = new Function(
  'const fn = () => {};\n' +
  'const f1 = () => {\n' +
  '  %PrepareFunctionForOptimization(fn);\n' +
  '  fn();\n'.repeat(49) +
  '};\n' +
  'return function main(prep) {\n' +
  '  if (prep) {\n' +
  '    %PrepareFunctionForOptimization(f1);\n' +
  '    f1();\n' +
  '    return;\n' +
  '  }\n' +
  '  f1();\n'.repeat(1338) +
  '};'
)();

%PrepareFunctionForOptimization(main);
main(true);
main(false);

%OptimizeFunctionOnNextCall(main);
assertEquals(undefined, main(false));
