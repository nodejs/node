const [e, c] = (() => {
  try {
    // Fails because functions can't be cloned
    structuredClone(()=>{});
  } catch (e) {
    // Return the original error and a clone
    return [e, structuredClone(e)];
  }
})();
console.log({
    'e instanceof Error': e instanceof Error,
    'e.name': e.name,
    'e.message': e.message,
    'e.stack': e.stack,
}, {
    'c instanceof Error': c instanceof Error,
    'c.name': c.name,
    'c.message': c.message,
    'c.stack': c.stack,
});
