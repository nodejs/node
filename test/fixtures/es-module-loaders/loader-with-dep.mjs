import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve ({specifier, parentURL}, defaultResolve, loader) {
  return {
    url: defaultResolve({specifier, parentURL}, defaultResolve, loader).url,
    format: dep.format
  };
}
