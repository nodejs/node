'use strict';

const common = require('../common');
if ((!common.hasCrypto) || (!common.hasIntl)) {
  common.skip('ESLint tests require crypto and Intl');
}

common.skipIfEslintMissing();

const RuleTester = require('../../tools/eslint/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/no-unused-promise-all-result');

function error(name, output) {
  return {
    message: `The fulfill value of \`${name}\` is not used, use \`SafePromiseAllReturnVoid\` instead.`,
    suggestions: output === undefined ? [] : [{
      desc: `Replace \`${name}\` with \`SafePromiseAllReturnVoid\`.`,
      output,
    }],
  };
}

new RuleTester({
  languageOptions: {
    ecmaVersion: 'latest',
    sourceType: 'script',
  },
}).run('no-unused-promise-all-result', rule, {
  valid: [
    'SafePromiseAllReturnVoid(promises)',
    'async function f() { await SafePromiseAllReturnVoid(promises); }',
    'const values = SafePromiseAll(promises)',
    'async function f() { const values = await SafePromiseAll(promises); }',
    'async function f() { return SafePromiseAll(promises); }',
    'async function f() { return await SafePromiseAllReturnArrayLike(promises); }',
    'async function f() { const { 0: a } = await PromiseAll(promises); }',
    'const f = () => SafePromiseAll(promises)',
    'foo(SafePromiseAll(promises))',
    'PromisePrototypeThen(SafePromiseAll(promises), (values) => {})',
    'PromisePrototypeThen(SafePromiseAll(promises), function({ 0: a }) {})',
    'const p = PromisePrototypeThen(SafePromiseAll(promises), undefined, onError)',
    'const p = PromisePrototypeCatch(SafePromiseAll(promises), onError)',
    'SafePromiseAll(promises).then((values) => {})',
    'const p = SafePromiseAll(promises).catch(onError)',
    'if (SafePromiseAll(promises)) {}',
    'SafePromiseAll(promises) ? a : b',
    'const x = (a, SafePromiseAll(promises))',
    'const x = a || SafePromiseAll(promises)',
    'SafePromiseAllSettled(promises)',
    'Promise.all(promises)',
    'PromisePrototypeThen(promise, () => SafePromiseAll(promises))',
  ],
  invalid: [
    {
      code: 'SafePromiseAll(promises)',
      errors: [error('SafePromiseAll', 'SafePromiseAllReturnVoid(promises)')],
    },
    {
      code: 'SafePromiseAllReturnArrayLike(promises, fn)',
      errors: [error('SafePromiseAllReturnArrayLike', 'SafePromiseAllReturnVoid(promises, fn)')],
    },
    {
      code: 'PromiseAll([a, b])',
      errors: [error('PromiseAll', 'SafePromiseAllReturnVoid([a, b])')],
    },
    {
      code: 'PromiseAll(iterable)',
      errors: [error('PromiseAll')],
    },
    {
      code: 'async function f() { await SafePromiseAll(promises); }',
      errors: [error('SafePromiseAll',
                     'async function f() { await SafePromiseAllReturnVoid(promises); }')],
    },
    {
      code: 'void SafePromiseAll(promises)',
      errors: [error('SafePromiseAll', 'void SafePromiseAllReturnVoid(promises)')],
    },
    {
      code: 'async function f() { cond ? await SafePromiseAll(a) : await PromiseAll([b]); }',
      errors: [
        error('SafePromiseAll',
              'async function f() { cond ? await SafePromiseAllReturnVoid(a) : await PromiseAll([b]); }'),
        error('PromiseAll',
              'async function f() { cond ? await SafePromiseAll(a) : await SafePromiseAllReturnVoid([b]); }'),
      ],
    },
    {
      code: 'cond && SafePromiseAll(promises)',
      errors: [error('SafePromiseAll', 'cond && SafePromiseAllReturnVoid(promises)')],
    },
    {
      code: 'const x = (SafePromiseAll(promises), 1)',
      errors: [error('SafePromiseAll', 'const x = (SafePromiseAllReturnVoid(promises), 1)')],
    },
    {
      code: 'PromisePrototypeThen(SafePromiseAll(promises), () => {})',
      errors: [error('SafePromiseAll', 'PromisePrototypeThen(SafePromiseAllReturnVoid(promises), () => {})')],
    },
    {
      code: 'const p = PromisePrototypeThen(SafePromiseAll(promises), function() {}, onError)',
      errors: [error('SafePromiseAll',
                     'const p = PromisePrototypeThen(SafePromiseAllReturnVoid(promises), function() {}, onError)')],
    },
    {
      code: 'PromisePrototypeThen(SafePromiseAll(promises), undefined, onError)',
      errors: [error('SafePromiseAll',
                     'PromisePrototypeThen(SafePromiseAllReturnVoid(promises), undefined, onError)')],
    },
    {
      code: 'async function f() { await SafePromisePrototypeFinally(SafePromiseAll(promises), cleanup); }',
      errors: [error(
        'SafePromiseAll',
        `async function f() {
          await SafePromisePrototypeFinally(SafePromiseAllReturnVoid(promises), cleanup);
        }`
      )],
    },
    {
      code: 'SafePromiseAll(promises).then(() => {})',
      errors: [error('SafePromiseAll', 'SafePromiseAllReturnVoid(promises).then(() => {})')],
    },
    {
      code: 'SafePromiseAll(promises).catch(onError)',
      errors: [error('SafePromiseAll', 'SafePromiseAllReturnVoid(promises).catch(onError)')],
    },
    {
      code: 'SafePromiseAll(promises)?.then()',
      errors: [error('SafePromiseAll', 'SafePromiseAllReturnVoid(promises)?.then()')],
    },
  ],
});
