const identifier = 'package-type-commonjs';
console.log(identifier);

if (!globalThis.eval_list) {
  globalThis.eval_list = [];
}

module.exports.foo = 42;
module.exports.identifier = identifier;

globalThis.eval_list.push('defer-1');
