export const glob = new WebAssembly.Global({ value: "i32" }, 42);
import { f } from "./js-wasm-cycle-global.wasm";
