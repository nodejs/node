function f() {
  return 42;
}
export { f };

import { mem, tab, glob, func } from "./js-wasm-cycle.wasm";
assert_false(glob instanceof WebAssembly.Global, "imported global should be unwrapped in ESM integration");
assert_equals(glob, 1, "unwrapped global should have direct value");
assert_true(mem instanceof WebAssembly.Memory);
assert_true(tab instanceof WebAssembly.Table);
assert_true(func instanceof Function);

f = () => {
  return 24;
};

assert_equals(func(), 42);
