/**
 * This file is combined with `loader-side-effect.mjs` via `--require`. Its
 * purpose is to test execution order of the two kinds of preload code.
 */

(globalThis.preloadOrder || (globalThis.preloadOrder = [])).push('--require');
