import { runMain } from 'node:module';

try { import.meta.resolve('doesnt-matter.mjs') } catch {}

runMain();

try { import.meta.resolve('doesnt-matter.mjs') } catch {}
