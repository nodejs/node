import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export async function resolve(specifier, context, nextResolve) {
  const { url } = await nextResolve(specifier, context);
  return {
    url,
    format: dep.format,
  };
}
