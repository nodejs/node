/** @type {[name: string, url: string][]} */
const importUrlTests = [
  ["Module URL", "./basic-module.js"],
  // In no-dynamic-import-in-module.any.js, this module is also statically imported
  ["Another module URL", "./basic-module-2.js"],
  [
    "Module data: URL",
    "data:text/javascript;charset=utf-8," +
      encodeURIComponent(`export default 'hello!';`),
  ],
];

for (const [name, url] of importUrlTests) {
  promise_test(
    (t) => promise_rejects_js(t, TypeError, import(url), "Import must reject"),
    name
  );
}
