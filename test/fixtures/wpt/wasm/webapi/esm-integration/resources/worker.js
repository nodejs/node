import * as mod from "./worker.wasm";
assert_true(await import("./worker.wasm") === mod);
