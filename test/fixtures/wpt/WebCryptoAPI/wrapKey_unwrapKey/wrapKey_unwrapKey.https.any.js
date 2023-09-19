// META: title=WebCryptoAPI: wrapKey() and unwrapKey()
// META: timeout=long
// META: script=../util/helpers.js

// Tests for wrapKey and unwrapKey round tripping

    var subtle = self.crypto.subtle;

    var wrappers = [];  // Things we wrap (and upwrap) keys with
    var keys = [];      // Things to wrap and unwrap

    // Generate all the keys needed, then iterate over all combinations
    // to test wrapping and unwrapping.
    promise_test(function() {
    return Promise.all([generateWrappingKeys(), generateKeysToWrap()])
    .then(function(results) {
        var promises = [];
        wrappers.forEach(function(wrapper) {
            keys.forEach(function(key) {
                promises.push(testWrapping(wrapper, key));
            })
        });
        return Promise.allSettled(promises);
    });
    }, "setup");

    function generateWrappingKeys() {
        // There are five algorithms that can be used for wrapKey/unwrapKey.
        // Generate one key with typical parameters for each kind.
        //
        // Note: we don't need cryptographically strong parameters for things
        // like IV - just any legal value will do.
        var parameters = [
            {
                name: "RSA-OAEP",
                generateParameters: {name: "RSA-OAEP", modulusLength: 4096, publicExponent: new Uint8Array([1,0,1]), hash: "SHA-256"},
                wrapParameters: {name: "RSA-OAEP", label: new Uint8Array(8)}
            },
            {
                name: "AES-CTR",
                generateParameters: {name: "AES-CTR", length: 128},
                wrapParameters: {name: "AES-CTR", counter: new Uint8Array(16), length: 64}
            },
            {
                name: "AES-CBC",
                generateParameters: {name: "AES-CBC", length: 128},
                wrapParameters: {name: "AES-CBC", iv: new Uint8Array(16)}
            },
            {
                name: "AES-GCM",
                generateParameters: {name: "AES-GCM", length: 128},
                wrapParameters: {name: "AES-GCM", iv: new Uint8Array(16), additionalData: new Uint8Array(16), tagLength: 128}
            },
            {
                name: "AES-KW",
                generateParameters: {name: "AES-KW", length: 128},
                wrapParameters: {name: "AES-KW"}
            }
        ];

        // Using allSettled to skip unsupported test cases.
        return Promise.allSettled(parameters.map(function(params) {
            return subtle.generateKey(params.generateParameters, true, ["wrapKey", "unwrapKey"])
            .then(function(key) {
                var wrapper;
                if (params.name === "RSA-OAEP") { // we have a key pair, not just a key
                    wrapper = {wrappingKey: key.publicKey, unwrappingKey: key.privateKey, parameters: params};
                } else {
                    wrapper = {wrappingKey: key, unwrappingKey: key, parameters: params};
                }
                wrappers.push(wrapper);
                return true;
            })
        }));
    }


    function generateKeysToWrap() {
        var parameters = [
            {algorithm: {name: "RSASSA-PKCS1-v1_5", modulusLength: 1024, publicExponent: new Uint8Array([1,0,1]), hash: "SHA-256"}, privateUsages: ["sign"], publicUsages: ["verify"]},
            {algorithm: {name: "RSA-PSS", modulusLength: 1024, publicExponent: new Uint8Array([1,0,1]), hash: "SHA-256"}, privateUsages: ["sign"], publicUsages: ["verify"]},
            {algorithm: {name: "RSA-OAEP", modulusLength: 1024, publicExponent: new Uint8Array([1,0,1]), hash: "SHA-256"}, privateUsages: ["decrypt"], publicUsages: ["encrypt"]},
            {algorithm: {name: "ECDSA", namedCurve: "P-256"}, privateUsages: ["sign"], publicUsages: ["verify"]},
            {algorithm: {name: "ECDH", namedCurve: "P-256"}, privateUsages: ["deriveBits"], publicUsages: []},
            {algorithm: {name: "Ed25519" }, privateUsages: ["sign"], publicUsages: ["verify"]},
            {algorithm: {name: "Ed448" }, privateUsages: ["sign"], publicUsages: ["verify"]},
            {algorithm: {name: "X25519" }, privateUsages: ["deriveBits"], publicUsages: []},
            {algorithm: {name: "X448" }, privateUsages: ["deriveBits"], publicUsages: []},
            {algorithm: {name: "AES-CTR", length: 128}, usages: ["encrypt", "decrypt"]},
            {algorithm: {name: "AES-CBC", length: 128}, usages: ["encrypt", "decrypt"]},
            {algorithm: {name: "AES-GCM", length: 128}, usages: ["encrypt", "decrypt"]},
            {algorithm: {name: "AES-KW", length: 128}, usages: ["wrapKey", "unwrapKey"]},
            {algorithm: {name: "HMAC", length: 128, hash: "SHA-256"}, usages: ["sign", "verify"]}
        ];

        // Using allSettled to skip unsupported test cases.
        return Promise.allSettled(parameters.map(function(params) {
            var usages;
            if ("usages" in params) {
                usages = params.usages;
            } else {
                usages = params.publicUsages.concat(params.privateUsages);
            }

            return subtle.generateKey(params.algorithm, true, usages)
            .then(function(result) {
                if (result.constructor === CryptoKey) {
                    keys.push({name: params.algorithm.name, algorithm: params.algorithm, usages: params.usages, key: result});
                } else {
                    keys.push({name: params.algorithm.name + " public key", algorithm: params.algorithm, usages: params.publicUsages, key: result.publicKey});
                    keys.push({name: params.algorithm.name + " private key", algorithm: params.algorithm, usages: params.privateUsages, key: result.privateKey});
                }
                return true;
            });
        }));
    }

    // Can we successfully "round-trip" (wrap, then unwrap, a key)?
    function testWrapping(wrapper, toWrap) {
        var formats;

        if (toWrap.name.includes("private")) {
            formats = ["pkcs8", "jwk"];
        } else if (toWrap.name.includes("public")) {
            formats = ["spki", "jwk"]
        } else {
            formats = ["raw", "jwk"]
        }

        return Promise.all(formats.map(function(fmt) {
            var originalExport;
            return subtle.exportKey(fmt, toWrap.key).then(function(exportedKey) {
                originalExport = exportedKey;
                const isPossible = wrappingIsPossible(originalExport, wrapper.parameters.name);
                promise_test(function(test) {
                    if (!isPossible) {
                        return Promise.resolve().then(() => {
                            assert_false(false, "Wrapping is not possible");
                        })
                    }
                    return subtle.wrapKey(fmt, toWrap.key, wrapper.wrappingKey, wrapper.parameters.wrapParameters)
                    .then(function(wrappedResult) {
                        return subtle.unwrapKey(fmt, wrappedResult, wrapper.unwrappingKey, wrapper.parameters.wrapParameters, toWrap.algorithm, true, toWrap.usages);
                    }).then(function(unwrappedResult) {
                        assert_goodCryptoKey(unwrappedResult, toWrap.algorithm, true, toWrap.usages, toWrap.key.type);
                        return subtle.exportKey(fmt, unwrappedResult)
                    }).then(function(roundTripExport) {
                        assert_true(equalExport(originalExport, roundTripExport), "Post-wrap export matches original export");
                    }, function(err) {
                        assert_unreached("Round trip for extractable key threw an error - " + err.name + ': "' + err.message + '"');
                    });
                }, "Can wrap and unwrap " + toWrap.name + " keys using " + fmt + " and " + wrapper.parameters.name);

                if (canCompareNonExtractableKeys(toWrap.key)) {
                    promise_test(function(test){
                        if (!isPossible) {
                            return Promise.resolve().then(() => {
                                assert_false(false, "Wrapping is not possible");
                            })
                        }
                        return subtle.wrapKey(fmt, toWrap.key, wrapper.wrappingKey, wrapper.parameters.wrapParameters)
                        .then(function(wrappedResult) {
                            return subtle.unwrapKey(fmt, wrappedResult, wrapper.unwrappingKey, wrapper.parameters.wrapParameters, toWrap.algorithm, false, toWrap.usages);
                        }).then(function(unwrappedResult){
                            assert_goodCryptoKey(unwrappedResult, toWrap.algorithm, false, toWrap.usages, toWrap.key.type);
                            return equalKeys(toWrap.key, unwrappedResult);
                        }).then(function(result){
                            assert_true(result, "Unwrapped key matches original");
                        }).catch(function(err){
                            assert_unreached("Round trip for key unwrapped non-extractable threw an error - " + err.name + ': "' + err.message + '"');
                        });
                    }, "Can wrap and unwrap " + toWrap.name + " keys as non-extractable using " + fmt + " and " + wrapper.parameters.name);

                    if (fmt === "jwk") {
                        promise_test(function(test){
                            if (!isPossible) {
                                return Promise.resolve().then(() => {
                                    assert_false(false, "Wrapping is not possible");
                                })
                            }
                            var wrappedKey;
                            return wrapAsNonExtractableJwk(toWrap.key,wrapper).then(function(wrappedResult){
                                wrappedKey = wrappedResult;
                                return subtle.unwrapKey("jwk", wrappedKey, wrapper.unwrappingKey, wrapper.parameters.wrapParameters, toWrap.algorithm, false, toWrap.usages);
                            }).then(function(unwrappedResult){
                                assert_false(unwrappedResult.extractable, "Unwrapped key is non-extractable");
                                return equalKeys(toWrap.key,unwrappedResult);
                            }).then(function(result){
                                assert_true(result, "Unwrapped key matches original");
                            }).catch(function(err){
                                assert_unreached("Round trip for non-extractable key threw an error - " + err.name + ': "' + err.message + '"');
                            }).then(function(){
                                return subtle.unwrapKey("jwk", wrappedKey, wrapper.unwrappingKey, wrapper.parameters.wrapParameters, toWrap.algorithm, true, toWrap.usages);
                            }).then(function(unwrappedResult){
                                assert_unreached("Unwrapping a non-extractable JWK as extractable should fail");
                            }).catch(function(err){
                                assert_equals(err.name, "DataError", "Unwrapping a non-extractable JWK as extractable fails with DataError");
                            });
                        }, "Can unwrap " + toWrap.name + " non-extractable keys using jwk and " + wrapper.parameters.name);
                    }
                }
            });
        }));
    }

    // Implement key wrapping by hand to wrap a key as non-extractable JWK
    function wrapAsNonExtractableJwk(key, wrapper){
        var wrappingKey = wrapper.wrappingKey,
            encryptKey;

        return subtle.exportKey("jwk",wrappingKey)
        .then(function(jwkWrappingKey){
            // Update the key generation parameters to work as key import parameters
            var params = Object.create(wrapper.parameters.generateParameters);
            if(params.name === "AES-KW") {
                params.name = "AES-CBC";
                jwkWrappingKey.alg = "A"+params.length+"CBC";
            } else if (params.name === "RSA-OAEP") {
                params.modulusLength = undefined;
                params.publicExponent = undefined;
            }
            jwkWrappingKey.key_ops = ["encrypt"];
            return subtle.importKey("jwk", jwkWrappingKey, params, true, ["encrypt"]);
        }).then(function(importedWrappingKey){
            encryptKey = importedWrappingKey;
            return subtle.exportKey("jwk",key);
        }).then(function(exportedKey){
            exportedKey.ext = false;
            var jwk = JSON.stringify(exportedKey)
            if (wrappingKey.algorithm.name === "AES-KW") {
                return aeskw(encryptKey, str2ab(jwk.slice(0,-1) + " ".repeat(jwk.length%8 ? 8-jwk.length%8 : 0) + "}"));
            } else {
                return subtle.encrypt(wrapper.parameters.wrapParameters,encryptKey,str2ab(jwk));
            }
        });
    }


    // RSA-OAEP can only wrap relatively small payloads. AES-KW can only
    // wrap payloads a multiple of 8 bytes long.
    function wrappingIsPossible(exportedKey, algorithmName) {
        if ("byteLength" in exportedKey && algorithmName === "AES-KW") {
            return exportedKey.byteLength % 8 === 0;
        }

        if ("byteLength" in exportedKey && algorithmName === "RSA-OAEP") {
            // RSA-OAEP can only encrypt payloads with lengths shorter
            // than modulusLength - 2*hashLength - 1 bytes long. For
            // a 4096 bit modulus and SHA-256, that comes to
            // 4096/8 - 2*(256/8) - 1 = 512 - 2*32 - 1 = 447 bytes.
            return exportedKey.byteLength <= 446;
        }

        if ("kty" in exportedKey && algorithmName === "AES-KW") {
            return JSON.stringify(exportedKey).length % 8 == 0;
        }

        if ("kty" in exportedKey && algorithmName === "RSA-OAEP") {
            return JSON.stringify(exportedKey).length <= 478;
        }

        return true;
    }


    // Helper methods follow:

    // Are two exported keys equal
    function equalExport(originalExport, roundTripExport) {
        if ("byteLength" in originalExport) {
            return equalBuffers(originalExport, roundTripExport);
        } else {
            return equalJwk(originalExport, roundTripExport);
        }
    }

    // Are two array buffers the same?
    function equalBuffers(a, b) {
        if (a.byteLength !== b.byteLength) {
            return false;
        }

        var aBytes = new Uint8Array(a);
        var bBytes = new Uint8Array(b);

        for (var i=0; i<a.byteLength; i++) {
            if (aBytes[i] !== bBytes[i]) {
                return false;
            }
        }

        return true;
    }

    // Are two Jwk objects "the same"? That is, does the object returned include
    // matching values for each property that was expected? It's okay if the
    // returned object has extra methods; they aren't checked.
    function equalJwk(expected, got) {
        var fields = Object.keys(expected);
        var fieldName;

        for(var i=0; i<fields.length; i++) {
            fieldName = fields[i];
            if (!(fieldName in got)) {
                return false;
            }
            if (objectToString(expected[fieldName]) !== objectToString(got[fieldName])) {
                return false;
            }
        }

        return true;
    }

    // Character representation of any object we may use as a parameter.
    function objectToString(obj) {
        var keyValuePairs = [];

        if (Array.isArray(obj)) {
            return "[" + obj.map(function(elem){return objectToString(elem);}).join(", ") + "]";
        } else if (typeof obj === "object") {
            Object.keys(obj).sort().forEach(function(keyName) {
                keyValuePairs.push(keyName + ": " + objectToString(obj[keyName]));
            });
            return "{" + keyValuePairs.join(", ") + "}";
        } else if (typeof obj === "undefined") {
            return "undefined";
        } else {
            return obj.toString();
        }

        var keyValuePairs = [];

        Object.keys(obj).sort().forEach(function(keyName) {
            var value = obj[keyName];
            if (typeof value === "object") {
                value = objectToString(value);
            } else if (typeof value === "array") {
                value = "[" + value.map(function(elem){return objectToString(elem);}).join(", ") + "]";
            } else {
                value = value.toString();
            }

            keyValuePairs.push(keyName + ": " + value);
        });

        return "{" + keyValuePairs.join(", ") + "}";
    }

    // Can we compare key values by using them
    function canCompareNonExtractableKeys(key){
        if (key.usages.indexOf("decrypt") !== -1) {
            return true;
        }
        if (key.usages.indexOf("sign") !== -1) {
            return true;
        }
        if (key.usages.indexOf("wrapKey") !== -1) {
            return true;
        }
        if (key.usages.indexOf("deriveBits") !== -1) {
            return true;
        }
        return false;
    }

    // Compare two keys by using them (works for non-extractable keys)
    function equalKeys(expected, got){
        if ( expected.algorithm.name !== got.algorithm.name ) {
            return Promise.resolve(false);
        }

        var cryptParams, signParams, wrapParams, deriveParams;
        switch(expected.algorithm.name){
            case "AES-CTR" :
                cryptParams = {name: "AES-CTR", counter: new Uint8Array(16), length: 64};
                break;
            case "AES-CBC" :
                cryptParams = {name: "AES-CBC", iv: new Uint8Array(16) };
                break;
            case "AES-GCM" :
                cryptParams = {name: "AES-GCM", iv: new Uint8Array(16) };
                break;
            case "RSA-OAEP" :
                cryptParams = {name: "RSA-OAEP", label: new Uint8Array(8) };
                break;
            case "RSASSA-PKCS1-v1_5" :
                signParams = {name: "RSASSA-PKCS1-v1_5"};
                break;
            case "RSA-PSS" :
                signParams = {name: "RSA-PSS", saltLength: 32 };
                break;
            case "ECDSA" :
                signParams = {name: "ECDSA", hash: "SHA-256"};
                break;
            case "Ed25519" :
                signParams = {name: "Ed25519"};
                break;
            case "Ed448" :
                signParams = {name: "Ed448"};
                break;
            case "X25519" :
                deriveParams = {name: "X25519"};
                break;
            case "X448" :
                deriveParams = {name: "X448"};
                break;
            case "HMAC" :
                signParams = {name: "HMAC"};
                break;
            case "AES-KW" :
                wrapParams = {name: "AES-KW"};
                break;
            case "ECDH" :
                deriveParams = {name: "ECDH"};
                break;
            default:
                throw new Error("Unsupported algorithm for key comparison");
        }

        if (cryptParams) {
            return subtle.exportKey("jwk",expected)
            .then(function(jwkExpectedKey){
                if (expected.algorithm.name === "RSA-OAEP") {
                    ["d","p","q","dp","dq","qi","oth"].forEach(function(field){ delete jwkExpectedKey[field]; });
                }
                jwkExpectedKey.key_ops = ["encrypt"];
                return subtle.importKey("jwk", jwkExpectedKey, expected.algorithm, true, ["encrypt"]);
            }).then(function(expectedEncryptKey){
                return subtle.encrypt(cryptParams, expectedEncryptKey, new Uint8Array(32));
            }).then(function(encryptedData){
                return subtle.decrypt(cryptParams, got, encryptedData);
            }).then(function(decryptedData){
                var result = new Uint8Array(decryptedData);
                return !result.some(x => x);
            });
        } else if (signParams) {
            var verifyKey;
            return subtle.exportKey("jwk",expected)
            .then(function(jwkExpectedKey){
                if (expected.algorithm.name === "RSA-PSS" || expected.algorithm.name === "RSASSA-PKCS1-v1_5") {
                    ["d","p","q","dp","dq","qi","oth"].forEach(function(field){ delete jwkExpectedKey[field]; });
                }
                if (expected.algorithm.name === "ECDSA" || expected.algorithm.name.startsWith("Ed")) {
                    delete jwkExpectedKey["d"];
                }
                jwkExpectedKey.key_ops = ["verify"];
                return subtle.importKey("jwk", jwkExpectedKey, expected.algorithm, true, ["verify"]);
            }).then(function(expectedVerifyKey){
                verifyKey = expectedVerifyKey;
                return subtle.sign(signParams, got, new Uint8Array(32));
            }).then(function(signature){
                return subtle.verify(signParams, verifyKey, signature, new Uint8Array(32));
            });
        } else if (wrapParams) {
            var aKeyToWrap, wrappedWithExpected;
            return subtle.importKey("raw", new Uint8Array(16), "AES-CBC", true, ["encrypt"])
            .then(function(key){
                aKeyToWrap = key;
                return subtle.wrapKey("raw", aKeyToWrap, expected, wrapParams);
            }).then(function(wrapResult){
                wrappedWithExpected = Array.from((new Uint8Array(wrapResult)).values());
                return subtle.wrapKey("raw", aKeyToWrap, got, wrapParams);
            }).then(function(wrapResult){
                var wrappedWithGot = Array.from((new Uint8Array(wrapResult)).values());
                return wrappedWithGot.every((x,i) => x === wrappedWithExpected[i]);
            });
        } else if (deriveParams) {
            var expectedDerivedBits;
            return subtle.generateKey(expected.algorithm, true, ['deriveBits']).then(({ publicKey }) => {
                deriveParams.public = publicKey;
                return subtle.deriveBits(deriveParams, expected, 128)
            })
            .then(function(result){
                expectedDerivedBits = Array.from((new Uint8Array(result)).values());
                return subtle.deriveBits(deriveParams, got, 128);
            }).then(function(result){
                var gotDerivedBits = Array.from((new Uint8Array(result)).values());
                return gotDerivedBits.every((x,i) => x === expectedDerivedBits[i]);
            });
        }
    }

    // Raw AES encryption
    function aes( k, p ) {
        return subtle.encrypt({name: "AES-CBC", iv: new Uint8Array(16) }, k, p).then(function(ciphertext){return ciphertext.slice(0,16);});
    }

    // AES Key Wrap
    function aeskw(key, data) {
        if (data.byteLength % 8 !== 0) {
            throw new Error("AES Key Wrap data must be a multiple of 8 bytes in length");
        }

        var A = Uint8Array.from([0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0, 0, 0, 0, 0, 0, 0, 0]),
            Av = new DataView(A.buffer),
            R = [],
            n = data.byteLength / 8;

        for(var i = 0; i<data.byteLength; i+=8) {
            R.push(new Uint8Array(data.slice(i,i+8)));
        }

        function aeskw_step(j, i, final, B) {
            A.set(new Uint8Array(B.slice(0,8)));
            Av.setUint32(4,Av.getUint32(4) ^ (n*j+i+1));
            R[i] = new Uint8Array(B.slice(8,16));
            if (final) {
                R.unshift(A.slice(0,8));
                var result = new Uint8Array(R.length * 8);
                R.forEach(function(Ri,i){ result.set(Ri, i*8); });
                return result;
            } else {
                A.set(R[(i+1)%n],8);
                return aes(key,A);
            }
        }

        var p = new Promise(function(resolve){
            A.set(R[0],8);
            resolve(aes(key,A));
        });

        for(var j=0;j<6;++j) {
            for(var i=0;i<n;++i) {
                p = p.then(aeskw_step.bind(undefined, j, i,j===5 && i===(n-1)));
            }
        }

        return p;
    }

    function str2ab(str)        { return Uint8Array.from( str.split(''), function(s){return s.charCodeAt(0)} ); }
    function ab2str(ab)         { return String.fromCharCode.apply(null, new Uint8Array(ab)); }

