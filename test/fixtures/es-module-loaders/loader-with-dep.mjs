import {createRequire} from '../../common/index.mjs';

const require = createRequire(import.meta.url);
const dep = require('./loader-dep.js');

export async function resolve(specifier, { parentUrl, importAssertions }, defaultResolve) {
  return {
    url: (await defaultResolve(specifier, { parentUrl, importAssertions }, defaultResolve)).url,
    format: dep.format
  };
}
