export default async function * customReporter(source) {
  const counters = {};
  for await (const event of source) {
    counters[event.type] = (counters[event.type] ?? 0) + 1;
  }
  yield "custom.mjs ";
  yield JSON.stringify(counters);
}
