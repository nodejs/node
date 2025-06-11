import * as mod from "module";

// This API is not available on v20.x. We are just checking that a
// using a --import preload to force the ESM loader to load CJS
// still handles CJS <-> CJS cycles just fine.
// mod.registerHooks({
//   load(url, context, nextLoad) {
//     return nextLoad(url, context);
//   },
// });
