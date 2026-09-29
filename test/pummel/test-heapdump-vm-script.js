'use strict';
require('../common');
const { validateByRetainingPath } = require('../common/heap');
const vm = require('vm');
const source = 'const foo = 123';
const script = new vm.Script(source);
const context = vm.createContext();

validateByRetainingPath('Node / ContextifyScript', [
  { node_name: '(shared function info)' },  // This is the UnboundScript referenced by ContextifyScript.
  { edge_name: 'script' },
  { edge_name: 'source', node_type: 'string', node_name: source },
]);

console.log(script, context); // Keep the script and context alive.
