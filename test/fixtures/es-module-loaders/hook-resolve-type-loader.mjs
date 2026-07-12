/** @type {Uint8Array} */
let data;
/** @type {number} */
let ESM_MODULE_INDEX;
/** @type {number} */
let CJS_MODULE_INDEX;

export function initialize({ sab, ESM_MODULE_INDEX:e, CJS_MODULE_INDEX:c }) {
  data = new Uint8Array(sab);
  ESM_MODULE_INDEX = e;
  CJS_MODULE_INDEX = c;
}

export async function resolve(specifier, context, next) {
  const nextResult = await next(specifier, context);
  const { format } = nextResult;

  if (format === 'module' || specifier.endsWith('.mjs')) {
    Atomics.add(data, ESM_MODULE_INDEX, 1);
  } else if (format == null || format === 'commonjs') {
    Atomics.add(data, CJS_MODULE_INDEX, 1);
  }

  return nextResult;
}
