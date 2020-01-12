import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve (specifier, { parentURL }, defaultResolve) {
  return {
    url: defaultResolve(specifier, {parentURL}, defaultResolve).url,
    format: dep.format
  };
}
