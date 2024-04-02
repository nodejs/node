import * as fixtures from '../../common/fixtures.mjs';
import { register } from 'node:module';

const sab = new SharedArrayBuffer(2);
const data = new Uint8Array(sab);

const ESM_MODULE_INDEX = 0
const CJS_MODULE_INDEX = 1

export function getModuleTypeStats() {
  const importedESM = Atomics.load(data, ESM_MODULE_INDEX);
  const importedCJS = Atomics.load(data, CJS_MODULE_INDEX);
  return { importedESM, importedCJS };
}

register(fixtures.fileURL('es-module-loaders/hook-resolve-type-loader.mjs'), {
  data: { sab, ESM_MODULE_INDEX, CJS_MODULE_INDEX },
});
