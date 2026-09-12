import { totalmem } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';

// Gate HTML generation for high-memory machines
//
// TODO(@avivkeller): Lower the amount of memory
// we use.
// TODO(@avivkeller): Fall back to WASM on machines
// without native implementations of our dependencies
const hasEnoughMemory = totalmem() > 5 * (1024 ** 3);
const canRunNative = !['s390x', 'ppc64'].includes(process.arch);

const fromRoot = (path) =>
  pathToFileURL(join(import.meta.dirname, '..', '..', path)).href;

export default {
  extends: '@node-core/doc-kit/config',

  target: ['legacy-json-all', canRunNative && hasEnoughMemory && 'section-pages'].filter(Boolean),

  global: {
    input: ['doc/api/*.md'],
    ignore: ['doc/api/quic.md'],
    output: 'out/doc/api',

    changelog: fromRoot('CHANGELOG.md'),
  },

  metadata: {
    typeMap: fromRoot('doc/type-map.json'),
  },
};
