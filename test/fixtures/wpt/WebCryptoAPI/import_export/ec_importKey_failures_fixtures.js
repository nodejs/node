function ecPrivateJwk(namedCurve) {
    return Object.assign({}, ecKeyData[namedCurve].jwk);
}

function getValidKeyData(algorithm) {
    var key = ecKeyData[algorithm.namedCurve];
    return [
        {format: "spki", data: key.spki},
        {format: "raw", data: key.raw},
        {format: "pkcs8", data: key.pkcs8},
        {format: "jwk", data: ecPrivateJwk(algorithm.namedCurve)}
    ];
}

function getBadKeyLengthData(algorithm) {
    var key = ecKeyData[algorithm.namedCurve];
    var jwk = ecPrivateJwk(algorithm.namedCurve);
    jwk.x = jwk.x.slice(0, -1);
    return [
        {format: "spki", data: key.spki.slice(0, -1)},
        {format: "raw", data: key.raw.slice(0, -1)},
        {format: "pkcs8", data: key.pkcs8.slice(0, -1)},
        {format: "jwk", data: jwk}
    ];
}

function getMissingJWKFieldKeyData(algorithm) {
    var missingX = ecPrivateJwk("P-521");
    var missingKty = ecPrivateJwk("P-521");
    var missingCrv = ecPrivateJwk("P-521");
    delete missingX.x;
    delete missingKty.kty;
    delete missingCrv.crv;
    return [
        {param: "x", data: missingX},
        {param: "kty", data: missingKty},
        {param: "crv", data: missingCrv}
    ];
}

function getMismatchedJWKKeyData(algorithm) {
    // TODO: Implement test cases where the public key doesn't match the private key.
    return [];
}

function getMismatchedKtyField(algorithm) {
    return "OKP";
}

function getMismatchedCrvField(algorithm) {
    return mismatchedEcCurves[algorithm.namedCurve];
}

var mismatchedEcCurves = {
    "P-521": "P-256",
    "P-256": "P-384",
    "P-384": "P-521"
};
