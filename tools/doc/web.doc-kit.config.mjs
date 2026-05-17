import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const ALL_PAGE_MEMORY_THRESHOLD = 4 * 1024 ** 3; // 4 GiB

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  'target': ['web', 'legacy-json-all'],

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
    generateAllPage: totalmem() > ALL_PAGE_MEMORY_THRESHOLD,
  },
};
