export function load(a, b, c) {
  return new Promise(d => setTimeout(() => d(c(a, b)), 99));
}
