export async function resolve(specifier, ctx, next) {
  const nextResult = await next(specifier, ctx);

  if (!specifier.endsWith('.txt')) return nextResult;

  return {
    format: 'txt',
    url: nextResult.url,
  };
}

export async function load(url, ctx, next) {
  const result = await next(url);

  if (result.format !== 'txt') return result;

  return {
    format: 'module',
    shortCircuit: true,
    source: `export default "${result.source} goodbye"`,
  };
}
