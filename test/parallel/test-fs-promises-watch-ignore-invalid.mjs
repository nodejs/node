import '../common/index.mjs';
import { skipIfNoWatch } from '../common/watch.js';

skipIfNoWatch();

const assert = await import('node:assert');
const { watch } = await import('node:fs/promises');

await assert.rejects(
  async () => {
    const watcher = watch('.', { ignore: 123 });
    // eslint-disable-next-line no-unused-vars
    for await (const _ of watcher) {
      // Will throw before yielding
    }
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  }
);

await assert.rejects(
  async () => {
    const watcher = watch('.', { ignore: '' });
    // eslint-disable-next-line no-unused-vars
    for await (const _ of watcher) {
      // Will throw before yielding
    }
  },
  {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  }
);
