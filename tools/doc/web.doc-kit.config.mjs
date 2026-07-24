import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

const hasEnoughMemory = totalmem() > 6 * (1024 ** 3); // 6GB

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  'target': ['legacy-json-all', hasEnoughMemory && 'web'].filter(Boolean),

  'global': {
    input: ['doc/api/*.md'],
    ignore: ['doc/api/quic.md'],
    output: 'out/doc/api',

    changelog: fromRoot('CHANGELOG.md'),
  },

  'metadata': {
    typeMap: fromRoot('doc/type-map.json'),
  },
};
