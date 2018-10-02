import {createRequire} from '../../common';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve (specifier, base, defaultResolve) {
  return {
    url: defaultResolve(specifier, base).url,
    format: dep.format
  };
}
