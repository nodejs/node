import { run } from 'node:test';
import { tap } from 'node:test/reporters';
import { parseArgs } from 'node:util';

const {
  values,
} = parseArgs({
  args: process.argv.slice(2),
  options: {
    file: { type: 'string' },
    only: { type: 'boolean' },
    'name-pattern': { type: 'string' },
    'skip-pattern': { type: 'string' },
  },
});

const opts = {
  isolation: 'none',
  files: [values.file],
};

if (values.only) {
  opts.only = true;
}
if (values['name-pattern']) {
  opts.testNamePatterns = [new RegExp(values['name-pattern'])];
}
if (values['skip-pattern']) {
  opts.testSkipPatterns = [new RegExp(values['skip-pattern'])];
}

run(opts).compose(tap).pipe(process.stdout);