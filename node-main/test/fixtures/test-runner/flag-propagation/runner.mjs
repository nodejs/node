import { run } from 'node:test';
import { tap } from 'node:test/reporters';
import { parseArgs } from 'node:util';

const options = {
  flag: {
    type: 'string',
    default: '',
  },
  expected: {
    type: 'string',
    default: '',
  },
  description: {
    type: 'string',
    default: 'flag propagation test',
  },
};

const { values } = parseArgs({ args: process.argv.slice(2), options });

const argv = [
  `--flag=${values.flag}`,
  `--expected=${values.expected}`,
  `--description="${values.description}"`,
].filter(Boolean);

run({
    files: ['./test.mjs'],
    cwd: process.cwd(),
    argv,
    isolation: 'process',
}).compose(tap).pipe(process.stdout);
