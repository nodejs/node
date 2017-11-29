import dep from './loader-dep.js';
export function resolve (specifier, base, defaultResolve) {
  return {
    url: defaultResolve(specifier, base).url,
    format: dep.format
  };
}
