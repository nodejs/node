import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export function resolve(specifier, { parentURL, importAssertions }, nextResolve) {
  return {
    url: (nextResolve(specifier, { parentURL, importAssertions })).url,
    format: dep.format
  };
}
