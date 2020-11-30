import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export async function resolve (specifier, { parentURL }, defaultResolve) {
  return {
    url: (await defaultResolve(specifier, {parentURL}, defaultResolve)).url,
    format: dep.format
  };
}
