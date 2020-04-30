import {ok, deepStrictEqual} from 'assert';

const dev = process.env.NODE_ENV === 'development';

export async function resolve(specifier, context, defaultResolve) {
  ok(Array.isArray(context.conditions), 'loader receives conditions array');
  deepStrictEqual(
    [...context.conditions].filter(c => c !== 'development').sort(),
    ['import', 'node']
  );
  return defaultResolve(specifier, {
    ...context,
    conditions: ['custom-condition', ...context.conditions],
  });
}
