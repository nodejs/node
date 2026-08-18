function runSignatureTests(options) {
  const subtle = self.crypto.subtle;
  const publicKeyCache = new WeakMap();
  const privateKeyCache = new WeakMap();
  const dataLabel = options.dataLabel || 'data';

  function algorithmIdentifier(vector) {
    return options.algorithmIdentifier(vector);
  }

  function algorithmName(vector) {
    const algorithm = algorithmIdentifier(vector);
    return typeof algorithm === 'string' ? algorithm : algorithm.name;
  }

  function algorithmWithNameGetter(vector, getter) {
    const algorithm = algorithmIdentifier(vector);
    const result = typeof algorithm === 'string' ? {} : { ...algorithm };
    Object.defineProperty(result, 'name', {
      enumerable: true,
      get: getter,
    });
    return result;
  }

  function importAlgorithm(vector) {
    return options.importAlgorithm
      ? options.importAlgorithm(vector)
      : {name: algorithmName(vector)};
  }

  function cachedKey(cache, vector, usages, importer) {
    let keys = cache.get(vector);
    if (keys === undefined) {
      keys = new Map();
      cache.set(vector, keys);
    }

    const cacheKey = usages.join(',');
    if (!keys.has(cacheKey)) {
      keys.set(cacheKey, importer());
    }
    return keys.get(cacheKey);
  }

  function publicKey(vector, usages = ['verify']) {
    return cachedKey(publicKeyCache, vector, usages, function () {
      return subtle.importKey(
        vector.publicKeyFormat || 'spki',
        vector.publicKeyBuffer,
        importAlgorithm(vector),
        false,
        usages
      );
    });
  }

  function privateKey(vector, usages = ['sign']) {
    return cachedKey(privateKeyCache, vector, usages, function () {
      return subtle.importKey(
        vector.privateKeyFormat || 'pkcs8',
        vector.privateKeyBuffer,
        importAlgorithm(vector),
        false,
        usages
      );
    });
  }

  async function assertInvalidAccess(operation, message) {
    let error;
    try {
      await operation();
    } catch (caught) {
      error = caught;
    }
    assert_not_equals(error, undefined, message);
    assert_equals(
      error.name,
      'InvalidAccessError',
      "Should have thrown InvalidAccessError instead of '" + error.message + "'"
    );
  }

  options.vectors.forEach(function (vector) {
    const algorithm = algorithmIdentifier(vector);

    promise_test(async function () {
      const key = await publicKey(vector);
      const isVerified = await subtle.verify(
        algorithm,
        key,
        vector.signature,
        vector.data
      );
      assert_true(isVerified, 'Signature verified');
    }, vector.name + ' verification');

    promise_test(async function () {
      const key = await publicKey(vector);
      const signature = copyBuffer(vector.signature);
      signature[0] = 255 - signature[0];
      const duringCallAlgorithm = algorithmWithNameGetter(vector, function () {
        signature[0] = vector.signature[0];
        return algorithmName(vector);
      });
      const isVerified = await subtle.verify(
        duringCallAlgorithm,
        key,
        signature,
        vector.data
      );
      assert_true(isVerified, 'Signature verified');
    }, vector.name + ' verification with altered signature during call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const signature = copyBuffer(vector.signature);
      const operation = subtle.verify(algorithm, key, signature, vector.data);
      signature[0] = 255 - signature[0];
      assert_true(await operation, 'Signature verified');
    }, vector.name + ' verification with altered signature after call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const signature = copyBuffer(vector.signature);
      const duringCallAlgorithm = algorithmWithNameGetter(vector, function () {
        signature.buffer.transfer();
        return algorithmName(vector);
      });
      const isVerified = await subtle.verify(
        duringCallAlgorithm,
        key,
        signature,
        vector.data
      );
      assert_false(isVerified, 'Signature is NOT verified');
    }, vector.name + ' verification with transferred signature during call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const signature = copyBuffer(vector.signature);
      const operation = subtle.verify(algorithm, key, signature, vector.data);
      signature.buffer.transfer();
      assert_true(await operation, 'Signature verified');
    }, vector.name + ' verification with transferred signature after call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const data = copyBuffer(vector.data);
      data[0] = 255 - data[0];
      const duringCallAlgorithm = algorithmWithNameGetter(vector, function () {
        data[0] = vector.data[0];
        return algorithmName(vector);
      });
      const isVerified = await subtle.verify(
        duringCallAlgorithm,
        key,
        vector.signature,
        data
      );
      assert_true(isVerified, 'Signature verified');
    }, vector.name + ' with altered ' + dataLabel + ' during call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const data = copyBuffer(vector.data);
      const operation = subtle.verify(algorithm, key, vector.signature, data);
      data[0] = 255 - data[0];
      assert_true(await operation, 'Signature verified');
    }, vector.name + ' with altered ' + dataLabel + ' after call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const data = copyBuffer(vector.data);
      const duringCallAlgorithm = algorithmWithNameGetter(vector, function () {
        data.buffer.transfer();
        return algorithmName(vector);
      });
      const isVerified = await subtle.verify(
        duringCallAlgorithm,
        key,
        vector.signature,
        data
      );
      assert_false(isVerified, 'Signature is NOT verified');
    }, vector.name + ' with transferred ' + dataLabel + ' during call');

    promise_test(async function () {
      const key = await publicKey(vector);
      const data = copyBuffer(vector.data);
      const operation = subtle.verify(algorithm, key, vector.signature, data);
      data.buffer.transfer();
      assert_true(await operation, 'Signature verified');
    }, vector.name + ' with transferred ' + dataLabel + ' after call');

    promise_test(async function () {
      const key = await privateKey(vector);
      await assertInvalidAccess(
        () => subtle.verify(algorithm, key, vector.signature, vector.data),
        'Using a private key to verify should fail'
      );
    }, vector.name + ' using privateKey to verify');

    promise_test(async function () {
      const key = await publicKey(vector);
      await assertInvalidAccess(
        () => subtle.sign(algorithm, key, vector.data),
        'Using a public key to sign should fail'
      );
    }, vector.name + ' using publicKey to sign');

    promise_test(async function () {
      const key = await publicKey(vector, []);
      await assertInvalidAccess(
        () => subtle.verify(algorithm, key, vector.signature, vector.data),
        'Verifying without the verify usage should fail'
      );
    }, vector.name + ' no verify usage');

    promise_test(async function () {
      const verificationKey = await publicKey(vector);
      const signingKey = await privateKey(vector);

      if (options.roundTrip) {
        await options.roundTrip({
          subtle,
          vector,
          algorithm,
          verificationKey,
          signingKey,
        });
        return;
      }

      if (options.katFirst) {
        const vectorSignatureIsVerified = await subtle.verify(
          algorithm,
          verificationKey,
          vector.signature,
          vector.data
        );
        assert_true(
          vectorSignatureIsVerified,
          'Known-answer signature verified'
        );
      }

      const signature = await subtle.sign(algorithm, signingKey, vector.data);

      if (!options.katFirst || !equalBuffers(signature, vector.signature)) {
        const generatedSignatureIsVerified = await subtle.verify(
          algorithm,
          verificationKey,
          signature,
          vector.data
        );
        assert_true(
          generatedSignatureIsVerified,
          'Generated signature verified'
        );
      }
    }, vector.name + ' round trip');

    promise_test(async function () {
      const wrongKey = options.wrongKey
        ? await options.wrongKey(vector, 'sign')
        : await subtle.generateKey(
            {name: 'HMAC', hash: 'SHA-1'},
            false,
            ['sign', 'verify']
          );
      await assertInvalidAccess(
        () => subtle.sign(algorithm, wrongKey, vector.data),
        'Signing with a key for another algorithm should fail'
      );
    }, vector.name + ' signing with wrong algorithm name');

    promise_test(async function () {
      const wrongKey = options.wrongKey
        ? await options.wrongKey(vector, 'verify')
        : await subtle.generateKey(
            {name: 'HMAC', hash: 'SHA-1'},
            false,
            ['sign', 'verify']
          );
      await assertInvalidAccess(
        () => subtle.verify(algorithm, wrongKey, vector.signature, vector.data),
        'Verifying with a key for another algorithm should fail'
      );
    }, vector.name +
      (options.wrongVerifyLabel || ' verifying with wrong algorithm name'));

    promise_test(async function () {
      const key = await publicKey(vector);
      const signature = copyBuffer(vector.signature);
      signature[0] = 255 - signature[0];
      const isVerified = await subtle.verify(
        algorithm,
        key,
        signature,
        vector.data
      );
      assert_false(isVerified, 'Signature NOT verified');
    }, vector.name +
      (options.alteredSignatureLabel ||
        ' verification failure due to altered signature'));

    if (options.shortSignature !== false) {
      promise_test(async function () {
        const key = await publicKey(vector);
        const signature = vector.signature.slice(1);
        const isVerified = await subtle.verify(
          algorithm,
          key,
          signature,
          vector.data
        );
        assert_false(isVerified, 'Signature NOT verified');
      }, vector.name + ' verification failure due to shortened signature');
    }

    promise_test(async function () {
      const key = await publicKey(vector);
      const data = copyBuffer(vector.data);
      data[0] = 255 - data[0];
      const isVerified = await subtle.verify(
        algorithm,
        key,
        vector.signature,
        data
      );
      assert_false(isVerified, 'Signature NOT verified');
    }, vector.name +
      (options.alteredDataLabel ||
        ' verification failure due to altered ' + dataLabel));

    if (options.generatedKeys) {
      promise_test(async function () {
        const key = await subtle.generateKey(algorithm, false, ['sign', 'verify']);
        const signature = await subtle.sign(
          algorithm,
          key.privateKey,
          vector.data
        );
        const isVerified = await subtle.verify(
          algorithm,
          key.publicKey,
          signature,
          vector.data
        );
        assert_true(isVerified, 'Verification failed.');
      }, 'Sign and verify using generated ' + algorithmName(vector) + ' keys.');
    }
  });

  (options.invalidVectors || []).forEach(function (vector) {
    promise_test(async function () {
      const isVerified = await subtle.verify(
        algorithmIdentifier(vector),
        await publicKey(vector),
        vector.signature,
        vector.data
      );
      assert_false(isVerified, 'Signature unexpectedly verified');
    }, vector.name + ' verification');
  });
}
