// META: title=WebCryptoAPI: BufferSource arguments and members reject shared buffers

// None of the BufferSource arguments of SubtleCrypto, or of the dictionaries it
// takes, are [AllowShared], so a SharedArrayBuffer, or a view onto one, has to
// throw a TypeError.
//
// See https://github.com/whatwg/html/issues/5380 for why not `new SharedArrayBuffer()`.
const sharedBuffer = new WebAssembly.Memory({ shared: true, initial: 1, maximum: 1 }).buffer;

function generateAesGcmKey() {
    return crypto.subtle.generateKey({ name: "AES-GCM", length: 128 }, false, ["encrypt"]);
}

for (const [kind, buffer] of [["SharedArrayBuffer", sharedBuffer],
                              ["Uint8Array(SharedArrayBuffer)", new Uint8Array(sharedBuffer)]]) {
    promise_test(t => {
        return promise_rejects_js(t, TypeError, crypto.subtle.digest("SHA-256", buffer));
    }, `digest() data is a ${kind}`);

    promise_test(async t => {
        const key = await generateAesGcmKey();
        await promise_rejects_js(t, TypeError,
            crypto.subtle.encrypt({ name: "AES-GCM", iv: new Uint8Array(12) }, key, buffer));
    }, `encrypt() data is a ${kind}`);

    promise_test(async t => {
        const key = await generateAesGcmKey();
        await promise_rejects_js(t, TypeError,
            crypto.subtle.encrypt({ name: "AES-GCM", iv: buffer }, key, new Uint8Array(1)));
    }, `encrypt() AesGcmParams.iv is a ${kind}`);

    promise_test(async t => {
        const key = await generateAesGcmKey();
        await promise_rejects_js(t, TypeError,
            crypto.subtle.encrypt({ name: "AES-GCM", iv: new Uint8Array(12), additionalData: buffer },
                                  key, new Uint8Array(1)));
    }, `encrypt() AesGcmParams.additionalData is a ${kind}`);
}
