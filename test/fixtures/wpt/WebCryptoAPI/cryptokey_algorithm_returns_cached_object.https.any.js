// META: title=WebCryptoAPI: CryptoKey.algorithm getter returns cached object

// https://w3c.github.io/webcrypto/#dom-cryptokey-algorithm
// https://github.com/servo/servo/issues/33908

promise_test(function() {
    return self.crypto.subtle.generateKey(
        {
          name: "AES-CTR",
          length: 256,
        },
        true,
        ["encrypt"],
      ).then(
        function(key) {
          let a = key.algorithm;
          let b = key.algorithm;
          assert_true(a === b);
        },
        function(err) {
            assert_unreached("generateKey threw an unexpected error: " + err.toString());
        }
    );
}, "CryptoKey.algorithm getter returns cached object");