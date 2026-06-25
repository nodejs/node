declare namespace InternalCryptoBinding {
  type Buffer = Uint8Array;
  type ByteSource = string | ArrayBuffer | SharedArrayBuffer | ArrayBufferView;
  type OptionalByteSource = ByteSource | undefined;
  type JwkKey = Record<string, string | string[] | boolean | undefined>;
  type KeyFormatDER = 0;
  type KeyFormatPEM = 1;
  type KeyFormatJWK = 2;
  type KeyFormatRawPublic = 3;
  type KeyFormatRawPrivate = 4;
  type KeyFormatRawSeed = 5;
  type PublicKeyFormat =
    KeyFormatDER | KeyFormatPEM | KeyFormatJWK | KeyFormatRawPublic | undefined;
  type PrivateKeyFormat =
    KeyFormatDER | KeyFormatPEM | KeyFormatJWK |
    KeyFormatRawPrivate | KeyFormatRawSeed | undefined;
  type KeyFormat = PublicKeyFormat | PrivateKeyFormat;
  type KeyEncoding = string | number | null | undefined;
  type KeyPassphrase = ByteSource | null | undefined;
  type NamedCurve = string | null | undefined;
  type PreparedAsymmetricKeyData = KeyObjectHandle | ByteSource | JwkKey;
  type PreparedSecretKeyData = KeyObjectHandle | ByteSource;
  type CryptoJobAsyncMode = 0;
  type CryptoJobSyncMode = 1;
  type CryptoJobWebCryptoMode = 2;
  type CryptoJobRegularMode = CryptoJobAsyncMode | CryptoJobSyncMode;
  type CryptoJobMode = CryptoJobRegularMode | CryptoJobWebCryptoMode;
  type CipherJobModeEncrypt = 0;
  type CipherJobModeDecrypt = 1;
  type CipherJobMode = CipherJobModeEncrypt | CipherJobModeDecrypt;
  type SignJobModeSign = 0;
  type SignJobModeVerify = 1;
  type SignJobMode = SignJobModeSign | SignJobModeVerify;
  type SignJobResult<M extends SignJobMode> =
    M extends SignJobModeSign ? ArrayBuffer : boolean;
  type MacJobSignatureArgs<M extends SignJobMode> =
    M extends SignJobModeVerify ? [signature: ByteSource] : [signature?: undefined];
  type SignJobSignatureArgs<M extends SignJobMode> =
    M extends SignJobModeVerify ? [signature: ByteSource] : [signature?: undefined];
  type KeyVariant = number;
  type KeyPairEncodingArgs<
    PublicFormat extends PublicKeyFormat = undefined,
    PrivateFormat extends PrivateKeyFormat = undefined,
  > = [
    publicFormat?: PublicFormat,
    publicType?: KeyEncoding,
    privateFormat?: PrivateFormat,
    privateType?: KeyEncoding,
    cipher?: string | null | undefined,
    passphrase?: KeyPassphrase,
  ];
  type CryptoJobResult<T> =
    [error: undefined, result: T] |
    [error: Error, result: undefined];

  interface CryptoJobAsync<T> {
    ondone?: (error: Error | undefined, result: T | undefined) => void;
    run(): void;
    getAsyncId?(): number;
  }

  interface CryptoJobSync<T> {
    run(): CryptoJobResult<T>;
    getAsyncId?(): number;
  }

  interface CryptoJobRegular<T> {
    ondone?: (error: Error | undefined, result: T | undefined) => void;
    run(): CryptoJobResult<T>;
    getAsyncId?(): number;
  }

  interface CryptoJobWebCrypto<T> {
    run(): Promise<T>;
  }

  type CryptoJob<T> = CryptoJobRegular<T> | CryptoJobWebCrypto<T>;
  type CryptoJobForMode<M extends CryptoJobMode, T> =
    [M] extends [CryptoJobWebCryptoMode] ? CryptoJobWebCrypto<T> :
      [M] extends [CryptoJobSyncMode] ? CryptoJobSync<T> :
        [M] extends [CryptoJobAsyncMode] ? CryptoJobAsync<T> :
          [M] extends [CryptoJobRegularMode] ? CryptoJobRegular<T> :
            CryptoJob<T>;

  type GeneratedPublicKey<Format extends PublicKeyFormat = PublicKeyFormat> =
    Format extends undefined ? KeyObjectHandle :
      Format extends KeyFormatPEM ? string :
        Format extends KeyFormatJWK ? JwkKey :
          Buffer;
  type GeneratedPrivateKey<Format extends PrivateKeyFormat = PrivateKeyFormat> =
    Format extends undefined ? KeyObjectHandle :
      Format extends KeyFormatPEM ? string :
        Format extends KeyFormatJWK ? JwkKey :
          Buffer;
  type GeneratedKey = GeneratedPublicKey | GeneratedPrivateKey;
  type GeneratedKeyPair<
    PublicFormat extends PublicKeyFormat = PublicKeyFormat,
    PrivateFormat extends PrivateKeyFormat = PrivateKeyFormat,
  > = [
    publicKey: GeneratedPublicKey<PublicFormat>,
    privateKey: GeneratedPrivateKey<PrivateFormat>,
  ];
  interface CryptoKey {
    readonly type: 'secret' | 'public' | 'private';
    readonly extractable: boolean;
    readonly algorithm: object;
    readonly usages: string[];
  }
  interface CryptoKeyConstructor {
    readonly prototype: CryptoKey;
    new(): CryptoKey;
  }
  interface InternalCryptoKeyConstructor {
    readonly prototype: CryptoKey;
    new(
      handle: KeyObjectHandle,
      algorithm: object | undefined,
      usagesMask: number,
      extractable: boolean,
    ): CryptoKey;
  }
  interface CryptoKeyPair {
    publicKey: CryptoKey;
    privateKey: CryptoKey;
  }

  type PreparedAsymmetricKeyArgs = [
    keyData: PreparedAsymmetricKeyData,
    keyFormat: KeyFormat,
    keyType: KeyEncoding,
    keyPassphrase: KeyPassphrase,
    keyNamedCurve: NamedCurve,
  ];

  interface Argon2JobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      message: ByteSource,
      nonce: ByteSource,
      parallelism: number,
      tagLength: number,
      memory: number,
      passes: number,
      secret: ByteSource,
      associatedData: ByteSource,
      type: number,
    ): CryptoJobForMode<M, ArrayBuffer>;
    new(
      mode: CryptoJobWebCryptoMode,
      message: KeyObjectHandle,
      nonce: ByteSource,
      parallelism: number,
      tagLength: number,
      memory: number,
      passes: number,
      secret: ByteSource,
      associatedData: ByteSource,
      type: number,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface AESCipherJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      cipherMode: CipherJobMode,
      key: KeyObjectHandle,
      data: ByteSource,
      variant: KeyVariant,
      iv?: ByteSource,
      tagLengthOrCounterLength?: number,
      additionalData?: OptionalByteSource,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface CShakeJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      algorithm: string,
      data: ByteSource,
      functionName: OptionalByteSource,
      customization: OptionalByteSource,
      outputLength: number,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface ChaCha20Poly1305CipherJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      cipherMode: CipherJobMode,
      key: KeyObjectHandle,
      data: ByteSource,
      iv: ByteSource,
      additionalData?: OptionalByteSource,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface CheckPrimeJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      candidate: ByteSource,
      checks: number,
    ): CryptoJobForMode<M, boolean>;
  }

  interface DHBitsJobConstructor {
    new<M extends CryptoJobMode>(
      mode: M,
      ...args: [
        ...publicKey: PreparedAsymmetricKeyArgs,
        ...privateKey: PreparedAsymmetricKeyArgs,
      ]
    ): CryptoJobForMode<M, ArrayBuffer>;
  }

  interface DhKeyPairGenJobConstructor {
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      group: string,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      primeOrPrimeLength: ByteSource | number,
      generator: number,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
  }

  interface DsaKeyPairGenJobConstructor {
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      modulusLength: number,
      divisorLength: number,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
  }

  interface EcKeyPairGenJobConstructor {
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      namedCurve: string,
      paramEncoding: number | null | undefined,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
    new(
      mode: CryptoJobWebCryptoMode,
      namedCurve: string,
      paramEncoding: null | undefined,
      algorithm: object,
      publicUsagesMask: number,
      privateUsagesMask: number,
      extractable: boolean,
    ): CryptoJobWebCrypto<CryptoKeyPair>;
  }

  interface HKDFJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      hash: string,
      key: ByteSource,
      salt: ByteSource,
      info: ByteSource,
      length: number,
    ): CryptoJobForMode<M, ArrayBuffer>;
    new(
      mode: CryptoJobWebCryptoMode,
      hash: string,
      key: KeyObjectHandle,
      salt: ByteSource,
      info: ByteSource,
      length: number,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface HashJobConstructor {
    new<M extends CryptoJobMode>(
      mode: M,
      algorithm: string,
      data: ByteSource,
      outputLength?: number,
    ): CryptoJobForMode<M, ArrayBuffer>;
  }

  interface HmacJobConstructor {
    new<S extends SignJobMode>(
      mode: CryptoJobWebCryptoMode,
      signMode: S,
      hash: string,
      key: KeyObjectHandle,
      data: ByteSource,
      ...signature: MacJobSignatureArgs<S>
    ): CryptoJobWebCrypto<SignJobResult<S>>;
  }

  type KemEncapsulateTuple = [
    sharedKey: Buffer,
    ciphertext: Buffer,
  ];

  interface EncapsulateResult {
    sharedKey: ArrayBuffer;
    ciphertext: ArrayBuffer;
  }

  interface KEMEncapsulateJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      ...key: PreparedAsymmetricKeyArgs
    ): CryptoJobForMode<M, KemEncapsulateTuple>;
    new(
      mode: CryptoJobWebCryptoMode,
      ...key: PreparedAsymmetricKeyArgs
    ): CryptoJobWebCrypto<EncapsulateResult>;
  }

  interface KEMDecapsulateJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      ...args: [
        ...key: PreparedAsymmetricKeyArgs,
        ciphertext: ByteSource,
      ]
    ): CryptoJobForMode<M, Buffer>;
    new(
      mode: CryptoJobWebCryptoMode,
      ...args: [
        ...key: PreparedAsymmetricKeyArgs,
        ciphertext: ByteSource,
      ]
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface KangarooTwelveJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      algorithm: string,
      customization: OptionalByteSource,
      outputLength: number,
      data: ByteSource,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface KmacJobConstructor {
    new<S extends SignJobMode>(
      mode: CryptoJobWebCryptoMode,
      signMode: S,
      key: KeyObjectHandle,
      algorithm: string,
      customization: OptionalByteSource,
      keyLength: number,
      outputLength: number,
      data: ByteSource,
      ...signature: MacJobSignatureArgs<S>
    ): CryptoJobWebCrypto<SignJobResult<S>>;
  }

  interface NidKeyPairGenJobConstructor {
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      nid: number,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
    new(
      mode: CryptoJobWebCryptoMode,
      nid: number,
      algorithm: object,
      publicUsagesMask: number,
      privateUsagesMask: number,
      extractable: boolean,
    ): CryptoJobWebCrypto<CryptoKeyPair>;
  }

  interface PBKDF2JobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      password: ByteSource,
      salt: ByteSource,
      iterations: number,
      length: number,
      digest: string,
    ): CryptoJobForMode<M, ArrayBuffer>;
    new(
      mode: CryptoJobWebCryptoMode,
      password: KeyObjectHandle,
      salt: ByteSource,
      iterations: number,
      length: number,
      digest: string,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface RandomBytesJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      buffer: ArrayBuffer | ArrayBufferView,
      offset: number,
      size: number,
    ): CryptoJobForMode<M, void>;
  }

  interface RandomPrimeJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      size: number,
      safe: boolean,
      add: OptionalByteSource,
      rem: OptionalByteSource,
    ): CryptoJobForMode<M, ArrayBuffer>;
  }

  interface RSACipherJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      cipherMode: CipherJobMode,
      key: KeyObjectHandle,
      data: ByteSource,
      variant: KeyVariant,
      digest: string,
      label?: OptionalByteSource,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface RsaKeyPairGenJobConstructor {
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      variant: KeyVariant,
      modulusLength: number,
      publicExponent: number,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
    new<
      M extends CryptoJobRegularMode,
      PublicFormat extends PublicKeyFormat = undefined,
      PrivateFormat extends PrivateKeyFormat = undefined,
    >(
      mode: M,
      variant: KeyVariant,
      modulusLength: number,
      publicExponent: number,
      hashAlgorithm: string | undefined,
      mgf1HashAlgorithm: string | undefined,
      saltLength: number | undefined,
      ...encoding: KeyPairEncodingArgs<PublicFormat, PrivateFormat>
    ): CryptoJobForMode<M, GeneratedKeyPair<PublicFormat, PrivateFormat>>;
    new(
      mode: CryptoJobWebCryptoMode,
      variant: KeyVariant,
      modulusLength: number,
      publicExponent: number,
      algorithm: object,
      publicUsagesMask: number,
      privateUsagesMask: number,
      extractable: boolean,
    ): CryptoJobWebCrypto<CryptoKeyPair>;
  }

  interface ScryptJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      password: ByteSource,
      salt: ByteSource,
      N: number,
      r: number,
      p: number,
      maxmem: number,
      length: number,
    ): CryptoJobForMode<M, ArrayBuffer>;
  }

  interface SecretKeyGenJobConstructor {
    new<M extends CryptoJobRegularMode>(
      mode: M,
      length: number,
    ): CryptoJobForMode<M, KeyObjectHandle>;
    new(
      mode: CryptoJobWebCryptoMode,
      length: number,
      algorithm: object,
      usagesMask: number,
      extractable: boolean,
    ): CryptoJobWebCrypto<CryptoKey>;
  }

  interface SignJobConstructor {
    new<M extends CryptoJobMode, S extends SignJobMode>(
      mode: M,
      signMode: S,
      ...args: [
        ...key: PreparedAsymmetricKeyArgs,
        data: ByteSource,
        digest: string | null | undefined,
        saltLength: number | undefined,
        padding: number | undefined,
        dsaEncoding: number | undefined,
        context: OptionalByteSource,
        ...signature: SignJobSignatureArgs<S>,
      ]
    ): CryptoJobForMode<M, SignJobResult<S>>;
  }

  interface TurboShakeJobConstructor {
    new(
      mode: CryptoJobWebCryptoMode,
      algorithm: string,
      domainSeparation: number,
      outputLength: number,
      data: ByteSource,
    ): CryptoJobWebCrypto<ArrayBuffer>;
  }

  interface KeyDetail {
    modulusLength?: number;
    publicExponent?: bigint;
    hashAlgorithm?: string;
    mgf1HashAlgorithm?: string;
    saltLength?: number;
    divisorLength?: number;
    namedCurve?: string;
    prime?: Buffer;
    generator?: Buffer;
  }

  interface KeyObjectHandle {
    init(
      type: number,
      data: PreparedAsymmetricKeyData,
      format?: number | null,
      typeOrAlgorithm?: string | number | null,
      cipherOrPassphrase?: string | ByteSource | null,
      passphraseOrNamedCurve?: ByteSource | string | null,
    ): void;
    getKeyType(): number;
    export(
      format?: number,
      type?: string | number | null,
      cipher?: string | null,
      passphrase?: ByteSource | null,
    ): string | Buffer;
    exportJwk(target: JwkKey, handleRsaPss: boolean): JwkKey;
    rawPublicKey(): Buffer;
    rawPrivateKey(): Buffer;
    rawSeed(): Buffer;
    exportECPublicRaw(format: number): Buffer;
    exportECPrivateRaw(): Buffer;
    keyDetail(target: KeyDetail): KeyDetail;
    equals(other: KeyObjectHandle): boolean;
    getAsymmetricKeyType(): string | undefined;
    getSymmetricKeySize(): number;
    checkEcKeyData(): boolean;
  }

  interface NativeKeyObject {
    new(handle: KeyObjectHandle): object;
  }
  interface NativeKeyObjectConstructor {
    readonly prototype: object;
    new(handle: KeyObjectHandle): object;
  }
  interface KeyObjectConstructor {
    readonly prototype: object;
    new(type: 'secret' | 'public' | 'private', handle: KeyObjectHandle): object;
  }
  interface KeyObjectSubtypeConstructor {
    readonly prototype: object;
    new(handle: KeyObjectHandle): object;
  }

  interface NativeCryptoKey {
    new(
      handle: KeyObjectHandle,
      algorithm: object | undefined,
      usagesMask: number,
      extractable: boolean,
    ): CryptoKey;
  }

  type KeyObjectSlots = [
    type: number,
    handle: KeyObjectHandle,
  ];

  type CryptoKeySlots = [
    type: number,
    extractable: boolean,
    algorithm: object,
    usagesMask: number,
    handle: KeyObjectHandle,
  ];

  type CreateNativeKeyObjectClassCallback =
    (NativeKeyObject: NativeKeyObjectConstructor) => [
      KeyObject: KeyObjectConstructor,
      SecretKeyObject: KeyObjectSubtypeConstructor,
      PublicKeyObject: KeyObjectSubtypeConstructor,
      PrivateKeyObject: KeyObjectSubtypeConstructor,
    ];

  type CreateCryptoKeyClassCallback =
    (NativeCryptoKey: NativeCryptoKey) =>
      [CryptoKeyConstructor, InternalCryptoKeyConstructor];

  interface HashHandle {
    update(data: ByteSource, encoding?: string): boolean;
    digest(encoding?: string): string | Buffer;
  }

  interface HmacHandle {
    init(algorithm: string, key: PreparedSecretKeyData): void;
    update(data: ByteSource, encoding?: string): boolean;
    digest(encoding?: string): string | Buffer;
  }

  interface CipherBaseHandle {
    update(data: ByteSource, inputEncoding?: string): Buffer;
    final(): Buffer;
    getAuthTag(): Buffer;
    setAuthTag(tag: ByteSource): boolean;
    setAAD(aad: ByteSource, plaintextLength?: number): boolean;
    setAutoPadding(autoPadding: boolean): boolean;
  }

  interface DiffieHellmanHandle {
    readonly verifyError: number;
    generateKeys(format?: number): Buffer;
    computeSecret(key: ByteSource): Buffer;
    getPrime(): Buffer;
    getGenerator(): Buffer;
    getPublicKey(format?: number): Buffer;
    getPrivateKey(): Buffer;
    setPublicKey(key: ByteSource): void;
    setPrivateKey(key: ByteSource): void;
  }

  interface SignHandle {
    init(algorithm: string): void;
    update(data: ByteSource, encoding?: string): void;
    sign(
      ...args: [
        ...key: PreparedAsymmetricKeyArgs,
        rsaPadding: number | undefined,
        pssSaltLength: number | undefined,
        dsaSigEnc: number,
      ]
    ): Buffer;
  }

  interface VerifyHandle {
    init(algorithm: string): void;
    update(data: ByteSource, encoding?: string): void;
    verify(
      ...args: [
        ...key: PreparedAsymmetricKeyArgs,
        signature: ByteSource,
        rsaPadding: number | undefined,
        pssSaltLength: number | undefined,
        dsaSigEnc: number,
      ]
    ): boolean;
  }

  interface SecureContextHandle {
    context?: object;
    init(secureProtocol: string | undefined, minVersion: number, maxVersion: number): void;
    setKey(key: ByteSource, passphrase?: ByteSource): void;
    setSigalgs(sigalgs: string): void;
    setEngineKey?(privateKeyIdentifier: string, privateKeyEngine: string): void;
    setCert(cert: ByteSource): void;
    setAllowPartialTrustChain(): void;
    addCACert(cert: ByteSource): void;
    addCRL(crl: ByteSource): void;
    addRootCerts(): void;
    setCipherSuites(cipherSuites: string): void;
    setCiphers(cipherList: string): void;
    setECDHCurve(ecdhCurve: string): void;
    setDHParam(dhparam: ByteSource | boolean): string | undefined;
    setMinProto(minVersion: number): void;
    setMaxProto(maxVersion: number): void;
    getMinProto(): number;
    getMaxProto(): number;
    setOptions(options: number): void;
    setSessionIdContext(sessionIdContext: string): void;
    setSessionTimeout(sessionTimeout: number): void;
    setCertificateCompression(algorithms: number): void;
    close(): void;
    loadPKCS12(pfx: ByteSource, passphrase?: ByteSource): void;
    setClientCertEngine(clientCertEngine: string): void;
    getTicketKeys(): Buffer;
    setTicketKeys(keys: ByteSource): void;
    enableTicketKeyCallback(): void;
    getCertificate(): Buffer | null;
    getIssuer(): Buffer | null;
  }

  interface X509CertificateHandle {
    subject(): string;
    subjectAltName(): string | undefined;
    issuer(): string;
    getIssuerCert(): X509CertificateHandle | undefined;
    infoAccess(): string | undefined;
    validFrom(): string;
    validTo(): string;
    validFromDate(): Date;
    validToDate(): Date;
    fingerprint(): string;
    fingerprint256(): string;
    fingerprint512(): string;
    keyUsage(): string[] | undefined;
    serialNumber(): string;
    signatureAlgorithm(): string | undefined;
    signatureAlgorithmOid(): string | undefined;
    raw(): Buffer;
    publicKey(): KeyObjectHandle;
    pem(): string;
    checkCA(): boolean;
    checkHost(host: string, flags?: number): string | undefined;
    checkEmail(email: string, flags?: number): string | undefined;
    checkIP(ip: string, flags?: number): string | undefined;
    checkIssued(otherCert: X509CertificateHandle): boolean;
    checkPrivateKey(pkey: KeyObjectHandle): boolean;
    verify(pkey: KeyObjectHandle): boolean;
    toLegacy(): object;
  }

  interface CipherInfo {
    name: string;
    nid: number;
    blockSize: number;
    ivLength: number;
    keyLength: number;
    mode: string;
  }

  type PublicKeyCipher = (
    ...args: [
      ...key: PreparedAsymmetricKeyArgs,
      buffer: ByteSource,
      padding: number,
      oaepHash: string | undefined,
      oaepLabel: OptionalByteSource,
    ]
  ) => Buffer;
}

export interface CryptoBinding {
  AESCipherJob: InternalCryptoBinding.AESCipherJobConstructor;
  Argon2Job: InternalCryptoBinding.Argon2JobConstructor;
  CShakeJob?: InternalCryptoBinding.CShakeJobConstructor;
  ChaCha20Poly1305CipherJob: InternalCryptoBinding.ChaCha20Poly1305CipherJobConstructor;
  CheckPrimeJob: InternalCryptoBinding.CheckPrimeJobConstructor;
  DHBitsJob: InternalCryptoBinding.DHBitsJobConstructor;
  DhKeyPairGenJob: InternalCryptoBinding.DhKeyPairGenJobConstructor;
  DsaKeyPairGenJob: InternalCryptoBinding.DsaKeyPairGenJobConstructor;
  EcKeyPairGenJob: InternalCryptoBinding.EcKeyPairGenJobConstructor;
  HKDFJob: InternalCryptoBinding.HKDFJobConstructor;
  HashJob: InternalCryptoBinding.HashJobConstructor;
  HmacJob: InternalCryptoBinding.HmacJobConstructor;
  KEMDecapsulateJob?: InternalCryptoBinding.KEMDecapsulateJobConstructor;
  KEMEncapsulateJob?: InternalCryptoBinding.KEMEncapsulateJobConstructor;
  KangarooTwelveJob: InternalCryptoBinding.KangarooTwelveJobConstructor;
  KmacJob: InternalCryptoBinding.KmacJobConstructor;
  NidKeyPairGenJob: InternalCryptoBinding.NidKeyPairGenJobConstructor;
  PBKDF2Job: InternalCryptoBinding.PBKDF2JobConstructor;
  RandomBytesJob: InternalCryptoBinding.RandomBytesJobConstructor;
  RandomPrimeJob: InternalCryptoBinding.RandomPrimeJobConstructor;
  RSACipherJob: InternalCryptoBinding.RSACipherJobConstructor;
  RsaKeyPairGenJob: InternalCryptoBinding.RsaKeyPairGenJobConstructor;
  ScryptJob?: InternalCryptoBinding.ScryptJobConstructor;
  SecretKeyGenJob: InternalCryptoBinding.SecretKeyGenJobConstructor;
  SignJob: InternalCryptoBinding.SignJobConstructor;
  TurboShakeJob: InternalCryptoBinding.TurboShakeJobConstructor;

  CipherBase: new (
    isEncrypt: boolean,
    cipher: string,
    credential: InternalCryptoBinding.PreparedSecretKeyData,
    iv: InternalCryptoBinding.ByteSource | null,
    authTagLength?: number,
  ) => InternalCryptoBinding.CipherBaseHandle;
  DiffieHellman: new (
    sizeOrKey: number | InternalCryptoBinding.ByteSource,
    generator?: number | InternalCryptoBinding.ByteSource,
  ) => InternalCryptoBinding.DiffieHellmanHandle;
  DiffieHellmanGroup: new (name: string) => InternalCryptoBinding.DiffieHellmanHandle;
  ECDH: new (curve: string) => InternalCryptoBinding.DiffieHellmanHandle;
  Hash: new (
    algorithm: string | InternalCryptoBinding.HashHandle,
    xofLen?: number,
    algorithmId?: number,
    algorithmCache?: Record<string, number>,
  ) => InternalCryptoBinding.HashHandle;
  Hmac: new () => InternalCryptoBinding.HmacHandle;
  KeyObjectHandle: new () => InternalCryptoBinding.KeyObjectHandle;
  SecureContext: new () => InternalCryptoBinding.SecureContextHandle;
  Sign: new () => InternalCryptoBinding.SignHandle;
  Verify: new () => InternalCryptoBinding.VerifyHandle;

  EVP_PKEY_ED25519: number;
  EVP_PKEY_ED448: number;
  EVP_PKEY_ML_DSA_44: number;
  EVP_PKEY_ML_DSA_65: number;
  EVP_PKEY_ML_DSA_87: number;
  EVP_PKEY_ML_KEM_512: number;
  EVP_PKEY_ML_KEM_768: number;
  EVP_PKEY_ML_KEM_1024: number;
  EVP_PKEY_SLH_DSA_SHA2_128F: number;
  EVP_PKEY_SLH_DSA_SHA2_128S: number;
  EVP_PKEY_SLH_DSA_SHA2_192F: number;
  EVP_PKEY_SLH_DSA_SHA2_192S: number;
  EVP_PKEY_SLH_DSA_SHA2_256F: number;
  EVP_PKEY_SLH_DSA_SHA2_256S: number;
  EVP_PKEY_SLH_DSA_SHAKE_128F: number;
  EVP_PKEY_SLH_DSA_SHAKE_128S: number;
  EVP_PKEY_SLH_DSA_SHAKE_192F: number;
  EVP_PKEY_SLH_DSA_SHAKE_192S: number;
  EVP_PKEY_SLH_DSA_SHAKE_256F: number;
  EVP_PKEY_SLH_DSA_SHAKE_256S: number;
  EVP_PKEY_X25519: number;
  EVP_PKEY_X448: number;
  OPENSSL_EC_EXPLICIT_CURVE: number;
  OPENSSL_EC_NAMED_CURVE: number;
  RSA_PKCS1_PSS_PADDING: number;
  X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT: number;
  X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS: number;
  X509_CHECK_FLAG_NEVER_CHECK_SUBJECT: number;
  X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS: number;
  X509_CHECK_FLAG_NO_WILDCARDS: number;
  X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS: number;
  kCryptoJobAsync: InternalCryptoBinding.CryptoJobAsyncMode;
  kCryptoJobSync: InternalCryptoBinding.CryptoJobSyncMode;
  kCryptoJobWebCrypto: InternalCryptoBinding.CryptoJobWebCryptoMode;
  kKeyEncodingPKCS1: number;
  kKeyEncodingPKCS8: number;
  kKeyEncodingSEC1: number;
  kKeyEncodingSPKI: number;
  kKeyFormatDER: InternalCryptoBinding.KeyFormatDER;
  kKeyFormatJWK: InternalCryptoBinding.KeyFormatJWK;
  kKeyFormatPEM: InternalCryptoBinding.KeyFormatPEM;
  kKeyFormatRawPrivate: InternalCryptoBinding.KeyFormatRawPrivate;
  kKeyFormatRawPublic: InternalCryptoBinding.KeyFormatRawPublic;
  kKeyFormatRawSeed: InternalCryptoBinding.KeyFormatRawSeed;
  kKeyTypePrivate: number;
  kKeyTypePublic: number;
  kKeyTypeSecret: number;
  kKeyVariantAES_CBC_128: number;
  kKeyVariantAES_CBC_192: number;
  kKeyVariantAES_CBC_256: number;
  kKeyVariantAES_CTR_128: number;
  kKeyVariantAES_CTR_192: number;
  kKeyVariantAES_CTR_256: number;
  kKeyVariantAES_GCM_128: number;
  kKeyVariantAES_GCM_192: number;
  kKeyVariantAES_GCM_256: number;
  kKeyVariantAES_KW_128: number;
  kKeyVariantAES_KW_192: number;
  kKeyVariantAES_KW_256: number;
  kKeyVariantAES_OCB_128: number;
  kKeyVariantAES_OCB_192: number;
  kKeyVariantAES_OCB_256: number;
  kKeyVariantRSA_OAEP: number;
  kKeyVariantRSA_PSS: number;
  kKeyVariantRSA_SSA_PKCS1_v1_5: number;
  kSigEncDER: number;
  kSigEncP1363: number;
  kSignJobModeSign: InternalCryptoBinding.SignJobModeSign;
  kSignJobModeVerify: InternalCryptoBinding.SignJobModeVerify;
  kTypeArgon2d: number;
  kTypeArgon2i: number;
  kTypeArgon2id: number;
  kWebCryptoCipherDecrypt: InternalCryptoBinding.CipherJobModeDecrypt;
  kWebCryptoCipherEncrypt: InternalCryptoBinding.CipherJobModeEncrypt;
  kWebCryptoKeyFormatPKCS8: number;
  kWebCryptoKeyFormatRaw: number;
  kWebCryptoKeyFormatSPKI: number;

  ECDHConvertKey(
    key: InternalCryptoBinding.ByteSource,
    curve: string,
    format: number,
  ): InternalCryptoBinding.Buffer | '';
  certExportChallenge(spkac: InternalCryptoBinding.ByteSource): InternalCryptoBinding.Buffer | '';
  certExportPublicKey(spkac: InternalCryptoBinding.ByteSource): InternalCryptoBinding.Buffer | '';
  certVerifySpkac(spkac: InternalCryptoBinding.ByteSource): boolean | '';
  createCryptoKeyClass(
    callback: InternalCryptoBinding.CreateCryptoKeyClassCallback,
  ): [
    CryptoKey: InternalCryptoBinding.CryptoKeyConstructor,
    InternalCryptoKey: InternalCryptoBinding.InternalCryptoKeyConstructor,
  ];
  createNativeKeyObjectClass(
    callback: InternalCryptoBinding.CreateNativeKeyObjectClassCallback,
  ): [
    KeyObject: InternalCryptoBinding.KeyObjectConstructor,
    SecretKeyObject: InternalCryptoBinding.KeyObjectSubtypeConstructor,
    PublicKeyObject: InternalCryptoBinding.KeyObjectSubtypeConstructor,
    PrivateKeyObject: InternalCryptoBinding.KeyObjectSubtypeConstructor,
  ];
  getBundledRootCertificates(): string[];
  getCachedAliases(): Record<string, number>;
  getCertificateCompressionAlgorithms(): string[];
  getCipherInfo(
    nameOrNid: string | number,
    keyLength?: number,
    ivLength?: number,
  ): InternalCryptoBinding.CipherInfo | undefined;
  getCiphers(): string[];
  getCryptoKeySlots(key: object): InternalCryptoBinding.CryptoKeySlots;
  getCurves(): string[];
  getExtraCACertificates(): string[];
  getFipsCrypto(): 0 | 1;
  getHashes(): string[];
  getKeyObjectSlots(key: object): InternalCryptoBinding.KeyObjectSlots;
  getOpenSSLSecLevelCrypto(): number | undefined;
  getSSLCiphers(): string[];
  getSystemCACertificates(): string[];
  getUserRootCertificates(): string[];
  oneShotDigest(
    algorithm: string,
    algorithmId: number,
    algorithmCache: Record<string, number>,
    input: InternalCryptoBinding.ByteSource,
    outputEncoding: string,
    outputEncodingId?: number,
    outputLength?: number,
  ): string | InternalCryptoBinding.Buffer;
  parseX509(data: InternalCryptoBinding.ByteSource): InternalCryptoBinding.X509CertificateHandle;
  privateDecrypt: InternalCryptoBinding.PublicKeyCipher;
  privateEncrypt: InternalCryptoBinding.PublicKeyCipher;
  publicDecrypt: InternalCryptoBinding.PublicKeyCipher;
  publicEncrypt: InternalCryptoBinding.PublicKeyCipher;
  resetRootCertStore(): void;
  secureBuffer(length: number): Uint8Array | undefined;
  secureHeapUsed(): bigint | undefined;
  setEngine?(engine: string, flags: number): void;
  setFipsCrypto(fips: boolean | number): void;
  startLoadingCertificatesOffThread(): void;
  testFipsCrypto(): 0 | 1;
  timingSafeEqual(
    a: ArrayBufferView | ArrayBuffer,
    b: ArrayBufferView | ArrayBuffer,
  ): boolean;
}
