import { runMain } from 'node:module';

try { await import.meta.resolve('doesnt-matter.mjs') } catch {}

runMain();

try { await import.meta.resolve('doesnt-matter.mjs') } catch {}
