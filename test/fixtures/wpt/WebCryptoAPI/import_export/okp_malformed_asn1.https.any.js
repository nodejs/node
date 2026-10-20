// META: title=WebCryptoAPI: OKP imports reject malformed ASN.1

const vectors = [
  {
    algorithm: "Ed25519",
    pkcs8: "302e020100300506032b6570042204200007070707070707070707070707070707070707070707070707070707070707",
    raw: "ff812f84e53570c36ae9479aad71e649c3cc4a04a6768632150febdd66a238a8",
    leadingZeroRaw: "00009314998a59a276e35cac5d39d9cf642b5add8fc428baca5d18a7b30cda40",
    spki: "302a300506032b6570032100ff812f84e53570c36ae9479aad71e649c3cc4a04a6768632150febdd66a238a8",
  },
  {
    algorithm: "X25519",
    pkcs8: "302e020100300506032b656e042204200007070707070707070707070707070707070707070707070707070707070707",
    raw: "13be4feaeaf204c7fd3358fc9c00721881d174278128227ec674f37f7fe97b6d",
    leadingZeroRaw: "00a3785afdc7bb07fa13a4b075f00576cba6946a70cd6ad20b25c949eae6f636",
    spki: "302a300506032b656e03210013be4feaeaf204c7fd3358fc9c00721881d174278128227ec674f37f7fe97b6d",
  },
];

function bytes(hex) {
  return new Uint8Array(hex.match(/../g).map(value => Number.parseInt(value, 16)));
}

function spki(algorithm, raw) {
  const oid = algorithm === "Ed25519" ? 0x70 : 0x6e;
  return new Uint8Array([
    0x30, raw.length + 10,
    0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, oid,
    0x03, raw.length + 1, 0x00,
    ...raw,
  ]);
}

for (const vector of vectors) {
  for (const format of ["pkcs8", "spki"]) {
    promise_test(async test => {
      const encoded = bytes(vector[format]);
      const usages = format === "pkcs8"
        ? (vector.algorithm === "Ed25519" ? ["sign"] : ["deriveBits"])
        : (vector.algorithm === "Ed25519" ? ["verify"] : []);
      await promise_rejects_dom(
        test,
        "DataError",
        crypto.subtle.importKey(format, encoded.slice(0, -1), vector.algorithm, true, usages),
        "truncated input",
      );
      const invalidTag = encoded.slice();
      invalidTag[0] = 0x31;
      await promise_rejects_dom(
        test,
        "DataError",
        crypto.subtle.importKey(format, invalidTag, vector.algorithm, true, usages),
        "invalid outer ASN.1 tag",
      );
    }, `${vector.algorithm} rejects malformed ${format}`);
  }
}

for (const vector of vectors) {
  const usages = vector.algorithm === "Ed25519" ? ["verify"] : [];
  const raw = bytes(vector.raw);
  const leadingZeroRaw = bytes(vector.leadingZeroRaw);

  promise_test(async () => {
    const key = await crypto.subtle.importKey(
      "spki",
      spki(vector.algorithm, leadingZeroRaw),
      vector.algorithm,
      true,
      usages,
    );
    assert_array_equals(
      new Uint8Array(await crypto.subtle.exportKey("raw", key)),
      leadingZeroRaw,
      "valid fixed-width public value",
    );
  }, `${vector.algorithm} accepts a valid 32-byte leading-zero SPKI public value`);

  const invalidPublicValues = [
    { name: "31-byte", raw: raw.slice(1) },
    { name: "33-byte", raw: new Uint8Array([...raw, 0x01]) },
    { name: "leading-zero 33-byte", raw: new Uint8Array([0x00, ...raw]) },
  ];
  for (const invalid of invalidPublicValues) {
    promise_test(async test => {
      await promise_rejects_dom(
        test,
        "DataError",
        crypto.subtle.importKey(
          "spki",
          spki(vector.algorithm, invalid.raw),
          vector.algorithm,
          true,
          usages,
        ),
      );
    }, `${vector.algorithm} rejects a ${invalid.name} SPKI public value`);
  }
}
