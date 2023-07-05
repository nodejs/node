export function resolve(url, _, next) {
  if (this != null) throw new Error('hook function must not be bound to ESMLoader instance');
  return next(url);
}

export function load(url, _, next) {
  if (this != null) throw new Error('hook function must not be bound to ESMLoader instance');
  return next(url);
}

export function globalPreload() {
  if (this != null) throw new Error('hook function must not be bound to ESMLoader instance');
  return "";
}
