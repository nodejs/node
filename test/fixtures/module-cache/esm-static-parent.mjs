// A parent module that statically imports esm-counter.mjs.
// Used by tests that verify memory retention of statically-linked modules.
export { count } from './esm-counter.mjs';
