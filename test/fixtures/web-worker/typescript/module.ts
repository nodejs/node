import { value } from './dependency.ts';

const result: number = await Promise.resolve(value);
postMessage({
  value: result,
  url: import.meta.url,
  main: import.meta.main,
  requireType: typeof require,
  thisIsUndefined: this === undefined,
});
