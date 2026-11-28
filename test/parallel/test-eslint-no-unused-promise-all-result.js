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
    suggestions: Number(output !== undefined),
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
    'async function f() { const values = await SafePromiseAllReturnArrayLike(promises); }',
    'async function f() { return SafePromiseAll(promises); }',
    'async function f() { return await SafePromiseAllReturnArrayLike(promises); }',
    'const f = () => SafePromiseAll(promises)',
    'foo(SafePromiseAll(promises))',
    'PromisePrototypeThen(SafePromiseAllReturnArrayLike(promises), (values) => {})',
    'PromisePrototypeThen(SafePromiseAllReturnArrayLike(promises), function({ 0: a }) {})',
    'const p = PromisePrototypeThen(SafePromiseAllReturnArrayLike(promises), undefined, onError)',
    'const p = PromisePrototypeCatch(SafePromiseAllReturnArrayLike(promises), onError)',
    'SafePromiseAllReturnArrayLike(promises).then((values) => {})',
    'const p = SafePromiseAllReturnArrayLike(promises).catch(onError)',
    'if (SafePromiseAllReturnArrayLike(promises)) {}',
    'SafePromiseAllReturnArrayLike(promises) ? a : b',
    'const x = (a, SafePromiseAllReturnArrayLike(promises))',
    'const x = a || SafePromiseAllReturnArrayLike(promises)',
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
        `async function f() { await SafePromisePrototypeFinally(SafePromiseAllReturnVoid(promises), cleanup); }`,
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

    {
      code: 'async function f() { const { 0: a, 1: b } = await SafePromiseAll(promises); }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const { 0: { 0: code, 1: signal } } = await SafePromiseAll(promises); }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const { length, ...rest } = await SafePromiseAll(promises); }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const values = await SafePromiseAll(promises); return values[0]; }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const values = await SafePromiseAll(promises, fn); ' +
            'for (let i = 0; i < values.length; i++) foo(values[i]); }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const values = await SafePromiseAll(promises); values[0].bar(); }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { const values = await SafePromiseAll(promises); values["1"]; }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'async function f() { return (await SafePromiseAll(promises))[0]; }',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'PromisePrototypeThen(SafePromiseAll(promises), ({ 0: a }) => a)',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'PromisePrototypeThen(SafePromiseAll(promises), function(values) { return values[0]; })',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
    {
      code: 'SafePromiseAll(promises).then((values) => values.length)',
      errors: [{
        message: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
        suggestions: 1,
      }],
    },
  ],
});
