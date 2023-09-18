export async function resolve(specifier, ctx, next) {
  const nextResult = await next(specifier, ctx);

  if (!specifier.endsWith('.mts')) return nextResult;

  return {
    format: 'typescript',
    url: nextResult.url,
  };
}


export async function load(url, ctx, next) {
  const result = await next(url);

  if (result.format !== 'typescript') return result;

  const stripped = result.source.toString().replace(/:\s*\w+\s*=/g, '=');

  return {
    format: 'module',
    source: stripped,
  };
}
