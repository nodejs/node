import { writeFileSync } from 'node:fs';

export function resolve(specifier, context, next) {
	writeFileSync(1, `resolve ${specifier}\n`);
  if (specifier === 'process-exit-module-resolve') {
    process.exit(42);
  }
	
  if (specifier === 'process-exit-module-load') {
    return { __proto__: null, shortCircuit: true, url: 'process-exit-on-load:///' }
  }
  return next(specifier, context);
}

export function load(url, context, next) {
	writeFileSync(1, `load ${url}\n`);
  if (url === 'process-exit-on-load:///') {
    process.exit(43);
  }
  return next(url, context);
}
