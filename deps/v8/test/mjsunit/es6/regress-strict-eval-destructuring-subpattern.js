// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// In strict mode, `eval` and `arguments` in destructuring sub-patterns and
// object rest properties must report `kStrictEvalArguments` rather than
// generic destructuring target errors.

const cases = [
  '"use strict"; [eval] = [];',
  '"use strict"; [arguments] = [];',
  '"use strict"; [...eval] = [];',
  '"use strict"; let [eval] = [];',
  '"use strict"; let [...eval] = [];',
  '"use strict"; ({a: eval} = {});',
  '"use strict"; ({a: arguments} = {});',
  '"use strict"; let {a: eval} = {};',
  '"use strict"; ({...eval} = {});',
  '"use strict"; ({...arguments} = {});',
  '"use strict"; let {...eval} = {};',
  '"use strict"; ([eval]) => {};',
  '"use strict"; ({a: eval}) => {};',
  '"use strict"; ({...eval}) => {};',
  '"use strict"; function f([eval]) {}',
  '"use strict"; function f({a: eval}) {}',
  '"use strict"; function f({...eval}) {}',
];

for (const code of cases) {
  assertThrows(
      () => eval(code),
      SyntaxError,
      "Unexpected eval or arguments in strict mode");
}
