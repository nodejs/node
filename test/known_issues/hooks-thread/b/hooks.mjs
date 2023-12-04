// This file will not get loaded because of a deadlock trying to resolve it.

export function resolve(specifier, context, nextResolve) {
  return nextResolve(specifier, context, nextResolve);
}

export function load(url, context, nextLoad) {
  return nextLoad(url, context, nextLoad);
}
