export function initialize() {
  if (this != null) {
    throw new Error('hook function must not be bound to loader instance');
  }
}

export function resolve(url, _, next) {
  if (this != null) {
    throw new Error('hook function must not be bound to loader instance');
  }

  return next(url);
}

export function load(url, _, next) {
  if (this != null) {
    throw new Error('hook function must not be bound to loader instance');
  }

  return next(url);
}
