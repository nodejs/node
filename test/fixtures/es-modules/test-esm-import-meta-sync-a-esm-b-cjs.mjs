// Module A (ESM) - imports B (CJS)
import { default as b } from './test-esm-import-meta-sync-b-cjs.cjs';
export const aValue = 'a-' + b.bValue;
