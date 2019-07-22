export function loadMissing() {
  return import('pkgexports/missing');
}

export function loadFromNumber() {
  return import('pkgexports-number/hidden.js');
}

export function loadDot() {
  return import('pkgexports');
}
