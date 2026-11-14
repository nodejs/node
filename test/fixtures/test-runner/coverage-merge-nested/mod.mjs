export const f = (a, b) => {
  if (a) {
    if (b) {
      return 'never';
    }
    return 'a';
  }
  return 'other';
};
