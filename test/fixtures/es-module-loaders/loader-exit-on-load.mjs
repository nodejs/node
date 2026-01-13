export function load(a, b, next) {
  if (a === 'data:exit') process.exit(42);
  return next(a, b);
}
