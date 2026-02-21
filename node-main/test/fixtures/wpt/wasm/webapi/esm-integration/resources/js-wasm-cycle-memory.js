export const mem = new WebAssembly.Memory({ initial: 10 });
import { f } from "./js-wasm-cycle-memory.wasm";
