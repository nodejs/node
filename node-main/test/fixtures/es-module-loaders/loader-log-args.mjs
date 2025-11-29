import { writeSync } from 'node:fs';
import { inspect } from 'node:util'

export async function resolve(...args) {
  writeSync(1, `resolve arg count: ${args.length}\n`);
  writeSync(1, inspect({
    specifier: args[0],
    context: args[1],
    next: args[2],
  }) + '\n');

  return {
    shortCircuit: true,
    url: args[0],
  };
}

export async function load(...args) {
  writeSync(1, `load arg count: ${args.length}\n`);
  writeSync(1, inspect({
    url: args[0],
    context: args[1],
    next: args[2],
  }) + '\n');

  return {
    format: 'module',
    source: '',
    shortCircuit: true,
  };
}
