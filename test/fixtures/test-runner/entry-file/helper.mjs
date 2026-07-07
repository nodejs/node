export async function runShared(t, target) {
  await t.test(`restore ${target}`, async () => {});
}
