// META: title=WebCryptoAPI: OKP imports preserve fixed-length key material

const message = bytes("010203");
const peer = {
  pkcs8: bytes("302e020100300506032b656e0422042038adaddfe94506a579a2cabcc9a50794b464689e065ed3e86b76e1ef1011f01a"),
  spki: bytes("302a300506032b656e032100a1d178cd56e2528f7b8d82ba2e3772b219c944f0e8fbfb0d1ca3951d0ce00b13"),
};

const vectors = [
  {
    algorithm: "Ed25519",
    name: "a leading-zero private key",
    pkcs8: "302e020100300506032b6570042204200007070707070707070707070707070707070707070707070707070707070707",
    raw: "ff812f84e53570c36ae9479aad71e649c3cc4a04a6768632150febdd66a238a8",
    signature: "94ff57055f3b90728d704bfbf19a3e4b49806f98a10384ae01ed5c700e6d482a12dd5f6372ce160f5cf94e8cd53555a7b525658d8f4126d463fa4200578d710e",
    spki: "302a300506032b6570032100ff812f84e53570c36ae9479aad71e649c3cc4a04a6768632150febdd66a238a8",
  },
  {
    algorithm: "Ed25519",
    name: "an all-zero private key",
    pkcs8: "302e020100300506032b6570042204200000000000000000000000000000000000000000000000000000000000000000",
    raw: "3b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29",
    signature: "2a26779ba6cbb5e54292257f725af112b273c38728329682d99ed81ba6d7670350ae4cc53c5456fa437128d19298a5d949ab46e3d41ab3dbcfb0b35c895e9304",
    spki: "302a300506032b65700321003b6a27bcceb6a42d62a3a8d02a6f0d73653215771de243a63ac048a18b59da29",
  },
  {
    algorithm: "Ed25519",
    name: "a leading-zero public key",
    pkcs8: "302e020100300506032b6570042204204dca5ad94292a81e64a2c780e98d5469f53523107d9b00a63e37b5097786481f",
    raw: "00009314998a59a276e35cac5d39d9cf642b5add8fc428baca5d18a7b30cda40",
    signature: "b6a45af39f7545b0fb894a583c5156e027a97cb09e46a999ffb269aec6fc0d0af76169c7771d09431c08ee22cb99c8966fa261097c843855cd2005678c88710a",
    spki: "302a300506032b657003210000009314998a59a276e35cac5d39d9cf642b5add8fc428baca5d18a7b30cda40",
  },
  {
    algorithm: "X25519",
    name: "a leading-zero private key",
    pkcs8: "302e020100300506032b656e042204200007070707070707070707070707070707070707070707070707070707070707",
    raw: "13be4feaeaf204c7fd3358fc9c00721881d174278128227ec674f37f7fe97b6d",
    secret: "51eafdc7ed3371472837de1aca43bee0b198aa02d1f619eb26ed1d1045bcd936",
    spki: "302a300506032b656e03210013be4feaeaf204c7fd3358fc9c00721881d174278128227ec674f37f7fe97b6d",
  },
  {
    algorithm: "X25519",
    name: "an all-zero private key",
    pkcs8: "302e020100300506032b656e042204200000000000000000000000000000000000000000000000000000000000000000",
    raw: "2fe57da347cd62431528daac5fbb290730fff684afc4cfc2ed90995f58cb3b74",
    secret: "caa5f83ef211ee3ce3ae25b011f85ba0c55384769049a04bfc19770373c6df30",
    spki: "302a300506032b656e0321002fe57da347cd62431528daac5fbb290730fff684afc4cfc2ed90995f58cb3b74",
  },
  {
    algorithm: "X25519",
    name: "a leading-zero public key",
    pkcs8: "302e020100300506032b656e0422042031821845335f559760a05a2e1d3b0fd6e603b6a2640d969cd29e28ea9c5dddd7",
    raw: "00a3785afdc7bb07fa13a4b075f00576cba6946a70cd6ad20b25c949eae6f636",
    secret: "83b1b1eafc349edfc4401760c9949bc2fa95fceebd51cd7d87c2fd4a6ac85d21",
    spki: "302a300506032b656e03210000a3785afdc7bb07fa13a4b075f00576cba6946a70cd6ad20b25c949eae6f636",
  },
];

function bytes(hex) {
  return new Uint8Array(hex.match(/../g).map(value => Number.parseInt(value, 16)));
}

function base64url(value) {
  return btoa(String.fromCharCode(...value)).replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");
}

function privateBytes(pkcs8) {
  return pkcs8.slice(-32);
}

for (const vector of vectors) {
  promise_test(async () => {
    const pkcs8 = bytes(vector.pkcs8);
    const privateUsages = vector.algorithm === "Ed25519" ? ["sign"] : ["deriveBits"];
    const privateKey = await crypto.subtle.importKey("pkcs8", pkcs8, vector.algorithm, true, privateUsages);

    assert_array_equals(
      new Uint8Array(await crypto.subtle.exportKey("pkcs8", privateKey)),
      pkcs8,
      "PKCS#8 round trip",
    );
    const jwk = await crypto.subtle.exportKey("jwk", privateKey);
    assert_equals(jwk.d, base64url(privateBytes(pkcs8)), "JWK private key bytes");
    assert_equals(jwk.x, base64url(bytes(vector.raw)), "JWK public key bytes");

    const spki = bytes(vector.spki);
    const publicUsages = vector.algorithm === "Ed25519" ? ["verify"] : [];
    const publicKey = await crypto.subtle.importKey("spki", spki, vector.algorithm, true, publicUsages);
    assert_array_equals(
      new Uint8Array(await crypto.subtle.exportKey("spki", publicKey)),
      spki,
      "SPKI round trip",
    );
    assert_array_equals(
      new Uint8Array(await crypto.subtle.exportKey("raw", publicKey)),
      bytes(vector.raw),
      "raw public key",
    );

    if (vector.algorithm === "Ed25519") {
      const signature = new Uint8Array(await crypto.subtle.sign("Ed25519", privateKey, message));
      assert_true(
        await crypto.subtle.verify("Ed25519", publicKey, signature, message),
        "generated signature verifies",
      );
      assert_true(
        await crypto.subtle.verify("Ed25519", publicKey, bytes(vector.signature), message),
        "independent reference signature verifies",
      );
      return;
    }

    const peerPublic = await crypto.subtle.importKey("spki", peer.spki, "X25519", false, []);
    assert_array_equals(
      new Uint8Array(await crypto.subtle.deriveBits({name: "X25519", public: peerPublic}, privateKey, 256)),
      bytes(vector.secret),
      "independent reference shared secret",
    );
    const peerPrivate = await crypto.subtle.importKey("pkcs8", peer.pkcs8, "X25519", false, ["deriveBits"]);
    assert_array_equals(
      new Uint8Array(await crypto.subtle.deriveBits({name: "X25519", public: publicKey}, peerPrivate, 256)),
      bytes(vector.secret),
      "imported public key shared secret",
    );
  }, `${vector.algorithm} preserves ${vector.name}`);
}
