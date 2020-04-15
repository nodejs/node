import {ok, deepStrictEqual} from 'assert';

const env =
    process.env.NODE_ENV === 'production' ? 'production' : 'development';

export async function resolve(specifier, context, defaultResolve) {
  ok(Array.isArray(context.conditions), 'loader receives conditions array');
  deepStrictEqual([...context.conditions].sort(), [env, 'import', 'node']);
  return defaultResolve(specifier, {
    ...context,
    conditions: ['custom-condition', ...context.conditions],
  });
}
