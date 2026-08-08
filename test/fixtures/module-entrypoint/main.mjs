import { entrypoint } from 'node:module';

console.log(JSON.stringify({
  entrypoint,
  matchesMain: import.meta.url === entrypoint,
}));
