// This fixture is imported as a module in a few tests for `import defer`
// to ensure that a module imported with `import defer` is only executed once
// and its evaluation is deffered.
import * as assert from 'assert';

export const foo = 42;

// The `eval_list` is initialised by the importing module,
// so by the time the fixture is executed, `eval_list` should
// already be initialised.
assert.deepEqual(globalThis.eval_list, []);

globalThis.eval_list.push('defer-1');
