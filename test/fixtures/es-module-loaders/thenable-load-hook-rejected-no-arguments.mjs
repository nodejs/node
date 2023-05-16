export function load () {
  let thenAlreadyAccessed = false;
  return {
    get then() {
      if (thenAlreadyAccessed) throw new Error('must not call');
      thenAlreadyAccessed = true;
      return (_, reject) => reject();
    }
  };
}
