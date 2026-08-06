import { run } from 'node:test';
import { fileURLToPath } from 'node:url';

const fixture = fileURLToPath(
  new URL('./console-output-attribution-mutates-env.mjs', import.meta.url),
);
const stream = run({ files: [fixture], isolation: 'none' });
let attributed = 0;

for await (const event of stream) {
  if ((event.type === 'test:stdout' || event.type === 'test:stderr') &&
      'testId' in event.data) {
    attributed++;
  }
}

process.stdout.write(`__attributed_count__:${attributed}\n`);
