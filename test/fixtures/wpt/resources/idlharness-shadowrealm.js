// TODO: it would be nice to support `idl_array.add_objects`
function fetch_text(url) {
    return fetch(url).then(function (r) {
        if (!r.ok) {
            throw new Error("Error fetching " + url + ".");
        }
        return r.text();
    });
}

/**
 * idl_test_shadowrealm is a promise_test wrapper that handles the fetching of the IDL, and
 * running the code in a `ShadowRealm`, avoiding repetitive boilerplate.
 *
 * @see https://github.com/tc39/proposal-shadowrealm
 * @param {String[]} srcs Spec name(s) for source idl files (fetched from
 *      /interfaces/{name}.idl).
 * @param {String[]} deps Spec name(s) for dependency idl files (fetched
 *      from /interfaces/{name}.idl). Order is important - dependencies from
 *      each source will only be included if they're already know to be a
 *      dependency (i.e. have already been seen).
 */
function idl_test_shadowrealm(srcs, deps) {
    const script_urls = [
        "/resources/testharness.js",
        "/resources/WebIDLParser.js",
        "/resources/idlharness.js",
    ];
    promise_setup(async t => {
        const realm = new ShadowRealm();
        // https://github.com/web-platform-tests/wpt/issues/31996
        realm.evaluate("globalThis.self = globalThis; undefined;");

        realm.evaluate(`
            globalThis.self.GLOBAL = {
                isWindow: function() { return false; },
                isWorker: function() { return false; },
                isShadowRealm: function() { return true; },
            }; undefined;
        `);

        const ss = await Promise.all(script_urls.map(url => fetch_text(url)));
        for (const s of ss) {
            realm.evaluate(s);
        }
        const specs = await Promise.all(srcs.concat(deps).map(spec => {
            return fetch_text("/interfaces/" + spec + ".idl");
        }));
        const idls = JSON.stringify(specs);

        const results = JSON.parse(await new Promise(
          realm.evaluate(`(resolve,reject) => {
              const idls = ${idls};
              add_completion_callback(function (tests, harness_status, asserts_run) {
                resolve(JSON.stringify(tests));
              });

              // Without the wrapping test, testharness.js will think it's done after it has run
              // the first idlharness test.
              test(() => {
                  const idl_array = new IdlArray();
                  for (let i = 0; i < ${srcs.length}; i++) {
                      idl_array.add_idls(idls[i]);
                  }
                  for (let i = ${srcs.length}; i < ${srcs.length + deps.length}; i++) {
                      idl_array.add_dependency_idls(idls[i]);
                  }
                  idl_array.test();
              }, "setup");
          }`)
        ));

        // We ran the tests in the ShadowRealm and gathered the results. Now treat them as if
        // we'd run them directly here, so we can see them.
        for (const {name, status, message} of results) {
            // TODO: make this an API in testharness.js - needs RFC?
            promise_test(t => {t.set_status(status, message); t.phase = t.phases.HAS_RESULT; t.done()}, name);
        }
    }, "outer setup");
}
// vim: set expandtab shiftwidth=4 tabstop=4 foldmarker=@{,@} foldmethod=marker:
