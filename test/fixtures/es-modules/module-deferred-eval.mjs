if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}

export const foo = 42;
globalThis.eval_list.push('defer-1');
