
describe("Parses all of the IDLs to produce the correct ASTs", function () {
    for (var i = 0, n = data.valid.idl.length; i < n; i++) {
        var idl = data.valid.idl[i], json = data.valid.json[i];
        var func = (function (idl, json) {
            return function () {
                try {
                    // the AST contains NaN and +/-Infinity that cannot be serialised to JSON
                    // the stored JSON ASTs use the same replacement function as is used below
                    // so we compare based on that
                    var diff = jsondiffpatch.diff(json, WebIDL2.parse(idl));
                    if (diff && debug) console.log(JSON.stringify(diff, null, 4));
                    expect(diff).toBe(undefined);
                }
                catch (e) {
                    console.log(e.toString());
                    throw e;
                }
            };
        }(idl, json));
        it("should produce the same AST for " + i, func);
    }
});

describe("Parses all of the invalid IDLs to check that they blow up correctly", function () {
    for (var i = 0, n = data.invalid.idl.length; i < n; i++) {
        var idl = data.invalid.idl[i], error = data.invalid.json[i];
        var func = (function (idl, err) {
            return function () {
                var error;
                try {
                    var ast = WebIDL2.parse(idl);
                    console.log(JSON.stringify(ast, null, 4));
                }
                catch (e) {
                    error = e;
                }
                finally {
                    expect(error).toExist();
                    expect(error.message).toEqual(err.message);
                    expect(error.line).toEqual(err.line);
                }

            };
        }(idl, error));
        it("should produce the right error for " + i, func);
    }
});
