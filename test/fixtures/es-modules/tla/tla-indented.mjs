export const ready = true;

if (ready) {
  await Promise.resolve(ready);
}

for await (const x of [Promise.resolve(1)]) {
  void x;
}
