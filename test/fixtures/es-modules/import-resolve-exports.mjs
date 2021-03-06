import { strictEqual } from 'assert';

(async () => {
  const resolved = await import.meta.resolve('pkgexports-sugar');
  strictEqual(typeof resolved, 'string');
})()
.catch((e) => {
  console.error(e);
  process.exit(1);
});
