export function load(url, context, next) {
  let thenAlreadyAccessed = false;
  return {
    get then() {
      if (thenAlreadyAccessed) throw new Error('must not call');
      thenAlreadyAccessed = true;
      return (resolve) => resolve(next(url, context));
    }
  };
}
