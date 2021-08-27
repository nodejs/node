import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve (specifier, { parentURL, importAssertions }, defaultResolve) {
  return {
    url: defaultResolve(specifier, {parentURL, importAssertions}, defaultResolve).url,
    format: dep.format
  };
}
