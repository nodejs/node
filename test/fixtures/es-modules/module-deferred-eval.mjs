if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}
globalThis.eval_list.push('defer-1');

export const foo = 42;

console.log('executed');
