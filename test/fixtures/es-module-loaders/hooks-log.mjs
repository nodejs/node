import { writeFileSync } from 'node:fs';

let initializeCount = 0;
let resolveCount = 0;
let loadCount = 0;

export function initialize() {
  writeFileSync(1, `initialize ${++initializeCount}\n`);
}

export function resolve(specifier, context, next) {
  writeFileSync(1, `hooked resolve ${++resolveCount} ${specifier} \n`);
  return next(specifier, context);
}

export function load(url, context, next) {
  writeFileSync(1, `hooked load ${++loadCount} ${url}\n`);
  return next(url, context);
}
