import source modSource from "./worker.wasm";
import { pm } from "./worker-helper.js";
assert_true(modSource instanceof WebAssembly.Module);
assert_true(await import.source("./worker.wasm") === modSource);

await WebAssembly.instantiate(modSource, {
  "./worker-helper.js": {
    "pm": pm
  }
});