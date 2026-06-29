import { setTimeout } from 'node:timers/promises';

if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}
globalThis.eval_list.push('defer-tla-1');

export const foo = 42;

await setTimeout(1);
console.log('executed');
