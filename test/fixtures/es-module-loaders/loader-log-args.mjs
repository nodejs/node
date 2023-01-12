import { writeSync } from 'node:fs';

export async function resolve(...args) {
  writeSync(1, `resolve arg count: ${args.length}\n`);
  writeSync(1, `${JSON.stringify({
    specifier: args[0],
    context: args[1],
    next: args[2],
  })}\n`);

  return {
    shortCircuit: true,
    url: args[0],
  };
}

export async function load(...args) {
  writeSync(1, `load arg count: ${args.length}`);
  writeSync(1, `${JSON.stringify({
    url: args[0],
    context: args[1],
    next: args[2],
  })}\n`);

  return {
    format: 'module',
    source: '',
    shortCircuit: true,
  };
}
