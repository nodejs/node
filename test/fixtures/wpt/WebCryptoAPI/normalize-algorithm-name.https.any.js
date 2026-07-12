// META: title=WebCryptoAPI: Algorithm normalization matches names ASCII case-insensitively
// U+212A is the Kelvin Sign

function makeSalt() {
  return crypto.getRandomValues(new Uint8Array(32));
}

async function makeKey(algorithm) {
  const keyData = new Uint8Array([]);
  return crypto.subtle.importKey("raw", keyData, algorithm, false, ["deriveBits"]);
}

promise_test(async (t) => {
  const algorithm = {
    name: "H\u212ADF",
    hash: "SHA-256",
    salt: makeSalt(),
    info: new TextEncoder().encode(''),
  };
  const key = await makeKey("HKDF");
  const p = crypto.subtle.deriveBits(algorithm, key, 256);
  return promise_rejects_dom(t, "NotSupportedError", p, algorithm.name);
}, `"H<U+212A>DF" does not match "HKDF"`);

promise_test(async (t) => {
  const algorithm = {
    name: "PB\u212ADF2",
    hash: "SHA-256",
    iterations: 1,
    salt: makeSalt(),
  };
  const key = await makeKey("PBKDF2");
  const p = crypto.subtle.deriveBits(algorithm, key, 256);
  return promise_rejects_dom(t, "NotSupportedError", p, algorithm.name);
}, `"PB<U+212A>DF2" does not match "PBKDF2"`);

promise_test(async (t) => {
  const algorithm = {name: "AES-\u212AW", length: 256};
  const p = crypto.subtle.generateKey(algorithm, false, ["wrapKey"]);
  return promise_rejects_dom(t, "NotSupportedError", p, algorithm.name);
}, `"AES-<U+212A>W" does not match "AES-KW"`);

promise_test(async (t) => {
  const algorithm = {
    name: "RSASSA-P\u212ACS1-V1_5",
    modulusLength: 2048,
    publicExponent: new Uint8Array([3]),
    hash: "SHA-256",
  };
  const p = crypto.subtle.generateKey(algorithm, false, ["sign"]);
  return promise_rejects_dom(t, "NotSupportedError", p, algorithm.name);
}, `"RSASSA-P<U+212A>CS1-V1_5" does not match "RSASSA-PKCS1-V1_5"`);
