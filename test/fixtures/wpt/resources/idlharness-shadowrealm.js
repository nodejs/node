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
        const specs = await Promise.all(srcs.concat(deps).map(spec => {
            return fetch_text("/interfaces/" + spec + ".idl");
        }));
        const idls = JSON.stringify(specs);
        await new Promise(
            realm.evaluate(`(resolve,reject) => {
                (async () => {
                    await import("/resources/testharness.js");
                    await import("/resources/WebIDLParser.js");
                    await import("/resources/idlharness.js");
                    const idls = ${idls};
                    const idl_array = new IdlArray();
                    for (let i = 0; i < ${srcs.length}; i++) {
                        idl_array.add_idls(idls[i]);
                    }
                    for (let i = ${srcs.length}; i < ${srcs.length + deps.length}; i++) {
                        idl_array.add_dependency_idls(idls[i]);
                    }
                    idl_array.test();
                })().then(resolve, (e) => reject(e.toString()));
            }`)
        );
        await fetch_tests_from_shadow_realm(realm);
    });
}
// vim: set expandtab shiftwidth=4 tabstop=4 foldmarker=@{,@} foldmethod=marker:
