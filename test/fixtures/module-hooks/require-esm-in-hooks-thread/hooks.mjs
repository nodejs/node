// Loaded on the module.register() hooks thread. It defines no hooks: require()-ing
// an ES module with imports while the hooks module is being loaded is enough.
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { result } = require('./esm-with-import.mjs');
if (result !== 'dep:string') {
  throw new Error(`Unexpected result: ${result}`);
}
