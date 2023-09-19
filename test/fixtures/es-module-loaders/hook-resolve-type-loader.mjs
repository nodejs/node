/** @type {MessagePort} */
let port;
export function initialize(data) {
  port = data.port;
}

export async function resolve(specifier, context, next) {
  const nextResult = await next(specifier, context);
  const { format } = nextResult;

  if (format === 'module' || specifier.endsWith('.mjs')) {
    port.postMessage({ type: 'module' });
  } else if (format == null || format === 'commonjs') {
    port.postMessage({ type: 'commonjs' });
  }

  return nextResult;
}
