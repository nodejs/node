import {ok, deepStrictEqual} from 'assert';

export async function resolve(specifier, context, defaultResolve) {
  ok(Array.isArray(context.conditions), 'loader receives conditions array');
  deepStrictEqual([...context.conditions].sort(), ['import', 'node']);
  return defaultResolve(specifier, {
    ...context,
    conditions: ['custom-condition', ...context.conditions],
  });
}
