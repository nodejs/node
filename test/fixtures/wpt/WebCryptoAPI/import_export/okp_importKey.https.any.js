// META: title=WebCryptoAPI: importKey() for OKP keys
// META: timeout=long
// META: script=../util/helpers.js

// Test importKey and exportKey for OKP algorithms. Only "happy paths" are
// currently tested - those where the operation should succeed.

    var subtle = crypto.subtle;

    var keyData = {
        "Ed25519": {
            spki: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 112, 3, 33, 0, 216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204]),
            raw: new Uint8Array([216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204]),
            pkcs8: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 112, 4, 34, 4, 32, 243, 200, 244, 196, 141, 248, 120, 20, 110, 140, 211, 191, 109, 244, 229, 14, 56, 155, 167, 7, 78, 21, 194, 53, 45, 205, 93, 48, 141, 76, 168, 31]),
            jwk: {
                crv: "Ed25519",
                d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
                x: "2OGJY9gJ1IfZVJrMrsZ0Ln7rok2KDTsUt-PK6gaJPcw",
                kty: "OKP"
            }
        },

        "Ed448": {
            spki: new Uint8Array([48, 67, 48, 5, 6, 3, 43, 101, 113, 3, 58, 0, 171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90, 128]),
            raw: new Uint8Array([171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90, 128]),
            pkcs8: new Uint8Array([48, 71, 2, 1, 0, 48, 5, 6, 3, 43, 101, 113, 4, 59, 4, 57, 14, 255, 3, 69, 140, 40, 224, 23, 156, 82, 29, 227, 18, 201, 105, 183, 131, 67, 72, 236, 171, 153, 26, 96, 227, 178, 233, 167, 158, 76, 217, 228, 128, 239, 41, 23, 18, 210, 200, 61, 4, 114, 114, 213, 201, 244, 40, 102, 79, 105, 109, 38, 112, 69, 143, 29, 46]),
            jwk: {
                crv: "Ed448",
                d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
                x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
                kty: "OKP"
            }
        },

        "X25519": {
            spki: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 110, 3, 33, 0, 28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151, 6]),
            raw: new Uint8Array([28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151, 6]),
            pkcs8: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 110, 4, 34, 4, 32, 200, 131, 142, 118, 208, 87, 223, 183, 216, 201, 90, 105, 225, 56, 22, 10, 221, 99, 115, 253, 113, 164, 210, 118, 187, 86, 227, 168, 27, 100, 255, 97]),
            jwk: {
                crv: "X25519",
                d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2E",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
                kty: "OKP"
            }
        },

        "X448": {
            spki: new Uint8Array([48, 66, 48, 5, 6, 3, 43, 101, 111, 3, 57, 0, 182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206, 111]),
            raw: new Uint8Array([182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206, 111]),
            pkcs8: new Uint8Array([48, 70, 2, 1, 0, 48, 5, 6, 3, 43, 101, 111, 4, 58, 4, 56, 88, 199, 210, 154, 62, 181, 25, 178, 157, 0, 207, 177, 145, 187, 100, 252, 109, 138, 66, 216, 241, 113, 118, 39, 43, 137, 242, 39, 45, 24, 25, 41, 92, 101, 37, 192, 130, 150, 113, 176, 82, 239, 7, 39, 83, 15, 24, 142, 49, 208, 204, 83, 191, 38, 146, 158]),
            jwk: {
                crv: "X448",
                d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
                x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
                kty: "OKP"
            }
        },

    };

    // combinations to test
    var testVectors = [
        {name: "Ed25519", privateUsages: ["sign"], publicUsages: ["verify"]},
        {name: "Ed448", privateUsages: ["sign"], publicUsages: ["verify"]},
        {name: "X25519",  privateUsages: ["deriveKey", "deriveBits"], publicUsages: []},
        {name: "X448",  privateUsages: ["deriveKey", "deriveBits"], publicUsages: []},
    ];

    // TESTS ARE HERE:
    // Test every test vector, along with all available key data
    testVectors.forEach(function(vector) {
        [true, false].forEach(function(extractable) {

            // Test public keys first
            allValidUsages(vector.publicUsages, true).forEach(function(usages) {
                ['spki', 'jwk', 'raw'].forEach(function(format) {
                    var algorithm = {name: vector.name};
                    var data = keyData[vector.name];
                    if (format === "jwk") { // Not all fields used for public keys
                        data = {jwk: {kty: keyData[vector.name].jwk.kty, crv: keyData[vector.name].jwk.crv, x: keyData[vector.name].jwk.x}};
                    }

                    testFormat(format, algorithm, data, vector.name, usages, extractable);

                    // Test for https://github.com/WICG/webcrypto-secure-curves/pull/24
                    if (format === "jwk" && extractable) {
                        testJwkAlgBehaviours(algorithm, data.jwk, vector.name, usages);
                    }
                });

            });

            // Next, test private keys
            allValidUsages(vector.privateUsages).forEach(function(usages) {
                ['pkcs8', 'jwk'].forEach(function(format) {
                    var algorithm = {name: vector.name};
                    var data = keyData[vector.name];

                    testFormat(format, algorithm, data, vector.name, usages, extractable);

                    // Test for https://github.com/WICG/webcrypto-secure-curves/pull/24
                    if (format === "jwk" && extractable) {
                        testJwkAlgBehaviours(algorithm, data.jwk, vector.name, usages);
                    }
                });
            });
        });
    });


    // Test importKey with a given key format and other parameters. If
    // extrable is true, export the key and verify that it matches the input.
    function testFormat(format, algorithm, keyData, keySize, usages, extractable) {
        promise_test(function(test) {
            return subtle.importKey(format, keyData[format], algorithm, extractable, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, algorithm, extractable, usages, (format === 'pkcs8' || (format === 'jwk' && keyData[format].d)) ? 'private' : 'public');
                if (!extractable) {
                    return;
                }

                return subtle.exportKey(format, key).
                then(function(result) {
                    if (format !== "jwk") {
                        assert_true(equalBuffers(keyData[format], result), "Round trip works");
                    } else {
                        assert_true(equalJwk(keyData[format], result), "Round trip works");
                    }
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                assert_unreached("Threw an unexpected error: " + err.toString());
            });
        }, "Good parameters: " + keySize.toString() + " bits " + parameterString(format, keyData[format], algorithm, extractable, usages));
    }

    // Test importKey/exportKey "alg" behaviours, alg is ignored upon import and alg is missing for Ed25519 and Ed448 JWK export
    // https://github.com/WICG/webcrypto-secure-curves/pull/24
    function testJwkAlgBehaviours(algorithm, keyData, crv, usages) {
        promise_test(function(test) {
            return subtle.importKey('jwk', { ...keyData, alg: 'this is ignored' }, algorithm, true, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");

                return subtle.exportKey('jwk', key).
                then(function(result) {
                    assert_equals(Object.keys(result).length, keyData.d ? 6 : 5, "Correct number of JWK members");
                    assert_equals(result.alg, undefined, 'No JWK "alg" member is present');
                    assert_true(equalJwk(keyData, result), "Round trip works");
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                assert_unreached("Threw an unexpected error: " + err.toString());
            });
        }, "Good parameters with ignored JWK alg: " + crv.toString() + " " + parameterString('jwk', keyData, algorithm, true, usages));
    }



    // Helper methods follow:

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
            if (expected[fieldName] !== got[fieldName]) {
                return false;
            }
        }

        return true;
    }

    // Build minimal Jwk objects from raw key data and algorithm specifications
    function jwkData(keyData, algorithm) {
        var result = {
            kty: "oct",
            k: byteArrayToUnpaddedBase64(keyData)
        };

        if (algorithm.name.substring(0, 3) === "AES") {
            result.alg = "A" + (8 * keyData.byteLength).toString() + algorithm.name.substring(4);
        } else if (algorithm.name === "HMAC") {
            result.alg = "HS" + algorithm.hash.substring(4);
        }
        return result;
    }

    // Jwk format wants Base 64 without the typical padding at the end.
    function byteArrayToUnpaddedBase64(byteArray){
        var binaryString = "";
        for (var i=0; i<byteArray.byteLength; i++){
            binaryString += String.fromCharCode(byteArray[i]);
        }
        var base64String = btoa(binaryString);

        return base64String.replace(/=/g, "");
    }

    // Convert method parameters to a string to uniquely name each test
    function parameterString(format, data, algorithm, extractable, usages) {
        if ("byteLength" in data) {
            data = "buffer(" + data.byteLength.toString() + ")";
        } else {
            data = "object(" + Object.keys(data).join(", ") + ")";
        }
        var result = "(" +
                        objectToString(format) + ", " +
                        objectToString(data) + ", " +
                        objectToString(algorithm) + ", " +
                        objectToString(extractable) + ", " +
                        objectToString(usages) +
                     ")";

        return result;
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
