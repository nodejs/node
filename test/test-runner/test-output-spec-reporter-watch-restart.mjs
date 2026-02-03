import '../common/index.mjs';
import { spec as SpecReporter } from 'node:test/reporters';
import assert from 'node:assert';

const reporter = new SpecReporter();
let output = '';

reporter.on('data', (chunk) => {
  output += chunk;
});

reporter.write({
  type: 'test:watch:restarted',
});

reporter.end();

assert.match(output, /Restarted at .+/);
