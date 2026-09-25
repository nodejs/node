function algorithmName(algorithm) {
    return algorithm.name || algorithm;
}

function privateJwk(name) {
    return Object.assign({}, okpKeyData[name].jwk);
}

function publicJwk(name) {
    var jwk = privateJwk(name);
    delete jwk.d;
    return jwk;
}

function getValidKeyData(algorithm) {
    var name = algorithmName(algorithm);
    var key = okpKeyData[name];
    return [
        {format: "spki", data: key.spki},
        {format: "pkcs8", data: key.pkcs8},
        {format: "raw", data: key.raw},
        {format: "jwk", data: privateJwk(name)},
        {format: "jwk", data: publicJwk(name)}
    ];
}

function getBadKeyLengthData(algorithm) {
    var name = algorithmName(algorithm);
    var key = okpKeyData[name];
    var badPrivateJwk = privateJwk(name);
    var badPublicJwk = publicJwk(name);
    badPrivateJwk.d = badPrivateJwk.d.slice(0, -1);
    badPublicJwk.x = badPublicJwk.x.slice(0, -1);
    return [
        {format: "spki", data: key.spki.slice(0, -1)},
        {format: "pkcs8", data: key.pkcs8.slice(0, -1)},
        {format: "raw", data: key.raw.slice(0, -1)},
        {format: "jwk", data: badPrivateJwk},
        {format: "jwk", data: badPublicJwk}
    ];
}

function getMissingJWKFieldKeyData(algorithm) {
    var name = algorithmName(algorithm);
    var missingX = privateJwk(name);
    var missingKty = privateJwk(name);
    var missingCrv = name === "Ed448" ? privateJwk(name) : publicJwk(name);
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
    var name = algorithmName(algorithm);
    var jwk = privateJwk(name);
    jwk.x = mismatchedPublicKeys[name];
    return [jwk];
}

function getMismatchedKtyField(algorithm) {
    return "EC";
}

function getMismatchedCrvField(algorithm) {
    return mismatchedCurves[algorithmName(algorithm)];
}

var mismatchedPublicKeys = {
    "Ed25519": "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
    "Ed448": "X9dEm1m0Yf0s54fsYWrUah2hNCSFpw4fig6nXYDpZ3jt8SR2m0bHBhvWeD3x5Q9s0foavq_oJWGA",
    "X25519": "hSDwCYkwp1R0i33ctD73Wg2_Og0mOBr066SpjqqbTmo",
    "X448": "mwj3zDG34+Z9ItWuoSEHSic70rg94Jxj+qc9LCLF2bvINmRyQdlT1AxbEtqIEg1TF3+A5TLEH6A"
};

var mismatchedCurves = {
    "Ed25519": "X25519",
    "X25519": "Ed25519",
    "Ed448": "X448",
    "X448": "Ed448"
};
