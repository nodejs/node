import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

// Gate HTML generation for high-memory machines
// 
// TODO(@avivkeller): Lower the amount of memory
// we use.
const hasEnoughMemory = totalmem() > 5 * (1024 ** 3);
const hasEnoughMemoryForAll = totalmem() > 7 * (1024 ** 3);

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  'extends': '@node-core/doc-kit/config',

  'target': ['legacy-json-all', hasEnoughMemory && 'html'].filter(Boolean),

  'global': {
    input: ['doc/api/*.md'],
    ignore: ['doc/api/quic.md'],
    output: 'out/doc/api',

    changelog: fromRoot('CHANGELOG.md'),
  },

  'metadata': {
    typeMap: fromRoot('doc/type-map.json'),
  },

  'jsx-ast': {
    generateAllPage: hasEnoughMemoryForAll,
  },
};
