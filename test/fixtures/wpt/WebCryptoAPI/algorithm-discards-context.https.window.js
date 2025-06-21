// META: title=WebCryptoAPI: Properties discard the context in algorithm normalization

let nextTest = 0;
let tests = {};
function closeChild(testId) {
  if (tests[testId]) {
    let {child, t} = tests[testId];
    delete tests[testId];
    document.body.removeChild(child);
    t.done();
  }
}

function runInChild(t, childScript) {
  let testId = nextTest++;
  const preamble = `
let testId = ${testId};
function closeChildOnAccess(obj, key) {
  const oldValue = obj[key];
  Object.defineProperty(obj, key, {get: () => {
    top.closeChild(testId);
    return oldValue;
  }});
}
`;
  childScript = preamble + childScript;

  let child = document.createElement("iframe");
  tests[testId] = {t, child};
  document.body.appendChild(child);
  let script = document.createElement("script");
  script.textContent = childScript;
  child.contentDocument.body.appendChild(script);
}

async_test((t) => {
  const childScript = `
let algorithm = {name: "AES-GCM", length: 128};
closeChildOnAccess(algorithm, "name");
crypto.subtle.generateKey(algorithm, true, ["encrypt", "decrypt"]);`;
  runInChild(t, childScript);
}, "Context is discarded in generateKey");

async_test((t) => {
  const childScript = `
let algorithm = {name: "AES-GCM"};
closeChildOnAccess(algorithm, "name");
crypto.subtle.importKey("raw", new Uint8Array(16), algorithm, true,
                        ["encrypt", "decrypt"]);`;
  runInChild(t, childScript);
}, "Context is discarded in importKey");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.generateKey(
     {name: "AES-GCM", length: 128}, true, ["encrypt", "decrypt"]);
  let algorithm = {name: "AES-GCM", iv: new Uint8Array(12)};
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.encrypt(algorithm, key, new Uint8Array());
})();`;
  runInChild(t, childScript);
}, "Context is discarded in encrypt");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.generateKey(
     {name: "AES-GCM", length: 128}, true, ["encrypt", "decrypt"]);
  let algorithm = {name: "AES-GCM", iv: new Uint8Array(12)};
  let encrypted = await crypto.subtle.encrypt(algorithm, key, new Uint8Array());
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.decrypt(algorithm, key, encrypted);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in decrypt");

async_test((t) => {
  const childScript = `
let algorithm = {name: "SHA-256"};
closeChildOnAccess(algorithm, "name");
crypto.subtle.digest(algorithm, new Uint8Array());`;
  runInChild(t, childScript);
}, "Context is discarded in digest");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.generateKey(
      {name: "ECDSA", namedCurve: "P-256"}, true, ["sign", "verify"]);
  let algorithm = {name: "ECDSA", hash: "SHA-256"};
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.sign(algorithm, key.privateKey, new Uint8Array());
})();`;
  runInChild(t, childScript);
}, "Context is discarded in sign");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.generateKey(
      {name: "ECDSA", namedCurve: "P-256"}, true, ["sign", "verify"]);
  let algorithm = {name: "ECDSA", hash: "SHA-256"};
  let data = new Uint8Array();
  let signature = await crypto.subtle.sign(algorithm, key.privateKey, data);
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.verify(algorithm, key.publicKey, signature, data);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in verify");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.importKey(
      "raw", new Uint8Array(16), "HKDF", false, ["deriveBits"]);
  let algorithm = {
      name: "HKDF",
      hash: "SHA-256",
      salt: new Uint8Array(),
      info: new Uint8Array(),
  };
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.deriveBits(algorithm, key, 16);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in deriveBits");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.importKey(
      "raw", new Uint8Array(16), "HKDF", false, ["deriveKey"]);
  let algorithm = {
      name: "HKDF",
      hash: "SHA-256",
      salt: new Uint8Array(),
      info: new Uint8Array(),
  };
  let derivedAlgorithm = {name: "AES-GCM", length: 128};
  closeChildOnAccess(algorithm, "name");
  crypto.subtle.deriveKey(algorithm, key, derivedAlgorithm, true,
                          ["encrypt", "decrypt"]);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in deriveKey");

async_test((t) => {
  const childScript = `
(async () => {
  let key = await crypto.subtle.importKey(
      "raw", new Uint8Array(16), "HKDF", false, ["deriveKey"]);
  let algorithm = {
      name: "HKDF",
      hash: "SHA-256",
      salt: new Uint8Array(),
      info: new Uint8Array(),
  };
  let derivedAlgorithm = {name: "AES-GCM", length: 128};
  closeChildOnAccess(derivedAlgorithm, "name");
  crypto.subtle.deriveKey(algorithm, key, derivedAlgorithm, true,
                          ["encrypt", "decrypt"]);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in deriveKey (2)");

async_test((t) => {
  const childScript = `
(async () => {
  let wrapKey = await crypto.subtle.generateKey(
      {name: "AES-GCM", length: 128}, true, ["wrapKey", "unwrapKey"]);
  let key = await crypto.subtle.generateKey(
      {name: "AES-GCM", length: 128}, true, ["encrypt", "decrypt"]);
  let wrapAlgorithm = {name: "AES-GCM", iv: new Uint8Array(12)};
  closeChildOnAccess(wrapAlgorithm, "name");
  crypto.subtle.wrapKey("raw", key, wrapKey, wrapAlgorithm);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in wrapKey");

async_test((t) => {
  const childScript = `
(async () => {
  let wrapKey = await crypto.subtle.generateKey(
      {name: "AES-GCM", length: 128}, true, ["wrapKey", "unwrapKey"]);
  let keyAlgorithm = {name: "AES-GCM", length: 128};
  let keyUsages = ["encrypt", "decrypt"];
  let key = await crypto.subtle.generateKey(keyAlgorithm, true, keyUsages);
  let wrapAlgorithm = {name: "AES-GCM", iv: new Uint8Array(12)};
  let wrapped = await crypto.subtle.wrapKey("raw", key, wrapKey, wrapAlgorithm);
  closeChildOnAccess(wrapAlgorithm, "name");
  crypto.subtle.unwrapKey(
      "raw", wrapped, wrapKey, wrapAlgorithm, keyAlgorithm, true, keyUsages);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in unwrapKey");

async_test((t) => {
  const childScript = `
(async () => {
  let wrapKey = await crypto.subtle.generateKey(
      {name: "AES-GCM", length: 128}, true, ["wrapKey", "unwrapKey"]);
  let keyAlgorithm = {name: "AES-GCM", length: 128};
  let keyUsages = ["encrypt", "decrypt"];
  let key = await crypto.subtle.generateKey(keyAlgorithm, true, keyUsages);
  let wrapAlgorithm = {name: "AES-GCM", iv: new Uint8Array(12)};
  let wrapped = await crypto.subtle.wrapKey("raw", key, wrapKey, wrapAlgorithm);
  closeChildOnAccess(keyAlgorithm, "name");
  crypto.subtle.unwrapKey(
      "raw", wrapped, wrapKey, wrapAlgorithm, keyAlgorithm, true, keyUsages);
})();`;
  runInChild(t, childScript);
}, "Context is discarded in unwrapKey (2)");
