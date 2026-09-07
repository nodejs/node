import value from './module-dependency.mjs';

postMessage({
  requireType: typeof require,
  thisIsUndefined: this === undefined,
  value,
});
