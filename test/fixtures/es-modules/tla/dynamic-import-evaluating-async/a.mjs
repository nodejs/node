// Suspend so that main.mjs goes on to start evaluating b.mjs
await import('./d.mjs');
// b.mjs has been started but its body has not run yet
const { x } = await import('./b.mjs');
// b.mjs is now fully evaluated
const again = await import('./b.mjs');
if (again.x !== x) throw new Error(`expected ${x}, got ${again.x}`);
export const a = () => x;
