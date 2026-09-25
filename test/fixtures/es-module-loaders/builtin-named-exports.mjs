import * as fixtures from '../../common/fixtures.mjs';
import { createRequire, register } from 'node:module';

const require = createRequire(import.meta.url);

const GET_BUILTIN = `$__get_builtin_hole_${Date.now()}`;
Object.defineProperty(globalThis, GET_BUILTIN, {
  value: builtinName => require(builtinName),
  enumerable: false,
  configurable: false,
});

register(fixtures.fileURL('es-module-loaders/builtin-named-exports-loader.mjs'), {
  data: {
    GET_BUILTIN,
  },
});
