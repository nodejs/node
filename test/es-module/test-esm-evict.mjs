import '../common/index.mjs';
import { strictEqual } from 'node:assert';
import { releaseLoadedModule } from 'node:module';

const specifier = 'data:application/javascript,export default globalThis.value;';

globalThis.value = 1;
const instance1 = await import(specifier);
strictEqual(instance1.default, 1);
globalThis.value = 2;
const instance2 = await import(specifier);
strictEqual(instance2.default, 1);

strictEqual(releaseLoadedModule(specifier, import.meta.url), true);
strictEqual(releaseLoadedModule(specifier, import.meta.url), false);
const instance3 = await import(specifier);
strictEqual(instance3.default, 2);
