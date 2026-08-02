# PKCS#11 fixture for `test/parallel/test-crypto-key-store-pkcs11.js`.
#
# Builds a SoftHSM2 token holding the key material that test expects, together
# with the OpenSSL configuration that activates pkcs11-provider against it.
# `shell.nix` exposes the result through the `NODE_TEST_PKCS11_*` environment
# variables when it is instantiated with `--arg pkcs11 true`.
{
  pkgs ? import ./pkgs.nix { },

  # pkcs11-provider is dlopen'd into the libcrypto Node.js itself links, so it
  # has to be built against that very OpenSSL. SoftHSM links OpenSSL too;
  # building it against the same one keeps a single libcrypto in the process.
  openssl ? (import ./sharedLibDeps.nix { inherit pkgs; }).openssl,

  pin ? "1234",
}:

let
  # An override that yields the very same derivation is free, the package still
  # comes from the binary cache. Skipping pkcs11-provider's test suite is what
  # would cost that cache hit, so only do it once the override has forced a
  # rebuild anyway. SoftHSM needs neither, it does not run tests at all.
  pkcs11-provider =
    let
      inherit (pkgs) pkcs11-provider;
      pkcs11-provider' = pkcs11-provider.override { inherit openssl; };
    in
    if pkcs11-provider != pkcs11-provider' then
      pkcs11-provider'.overrideAttrs {
        doCheck = false;
      }
    else
      pkcs11-provider;

  softhsm = pkgs.softhsm.override { inherit openssl; };

  # OpenSSL names its provider modules after the platform's shared library
  # extension, while SoftHSM is a libtool module and keeps the `.so` suffix
  # everywhere, macOS included.
  providerModule = "${pkcs11-provider}/lib/ossl-modules/pkcs11${pkgs.stdenv.hostPlatform.extensions.sharedLibrary}";
  softhsmModule = "${softhsm}/lib/softhsm/libsofthsm2.so";

  # SoftHSM resolves a relative `directories.tokendir` against the working
  # directory, so naming it that way keeps this a plain store file rather than
  # something that has to be generated once the token's location is known.
  softhsmConf = pkgs.writeText "softhsm2.conf" ''
    directories.tokendir = tokens
    objectstore.backend = file
    log.level = ERROR
    slots.removable = false
  '';
in
{
  inherit
    pkcs11-provider
    softhsm
    ;

  opensslConf = pkgs.writeText "openssl-pkcs11.cnf" ''
    nodejs_conf = nodejs_init

    [nodejs_init]
    providers = provider_sect

    [provider_sect]
    default = default_sect
    pkcs11 = pkcs11_sect

    [default_sect]
    activate = 1

    [pkcs11_sect]
    module = ${providerModule}
    pkcs11-module-path = ${softhsmModule}
    pkcs11-module-quirks = no-deinit
    activate = 1
  '';

  # A SoftHSM working directory: the configuration above next to the token it
  # names. SoftHSM opens its token read-write, which a store path can never be,
  # so this is meant to be copied somewhere writable and used from there.
  #
  # `pkcs11-tool` comes from OpenSC and only fills the token here; neither it
  # nor `softhsm2-util` is needed to run the test.
  softhsmDir =
    pkgs.runCommand "node-pkcs11-softhsm"
      {
        nativeBuildInputs = [
          softhsm
          pkgs.opensc
        ];

        passthru = {
          inherit pin;
        };
      }
      ''
        export SOFTHSM2_CONF=${softhsmConf}
        mkdir tokens

        softhsm2-util --init-token --free --label node-test \
          --pin "${pin}" --so-pin "${pin}"

        keypairgen() {
          pkcs11-tool --module "${softhsmModule}" --login --pin "${pin}" \
            --keypairgen --key-type "$1" --id "$2" --label "$3" "$4"
        }

        # Keep this fixture limited to key types that SoftHSM and
        # pkcs11-provider can both generate and operate. Node's PQC APIs
        # require OpenSSL >= 3.5, but that does not imply ML-DSA or ML-KEM
        # support in this PKCS#11 stack.
        #
        # RSA decryption is likewise not covered: pkcs11-provider fails every
        # padding mode with `provider asym cipher failure` even for a key whose
        # PKCS#11 attributes include the decrypt usage, so
        # crypto.privateDecrypt() cannot be exercised against this stack.
        keypairgen RSA:2048 01 node-rsa --usage-sign
        keypairgen EC:prime256v1 02 node-ec --usage-sign
        keypairgen EC:ED25519 03 node-ed25519 --usage-sign
        keypairgen EC:ED448 04 node-ed448 --usage-sign
        keypairgen EC:prime256v1 05 node-ecdh --usage-derive

        mkdir -p "$out"
        cp -R tokens "$out/tokens"
        cp ${softhsmConf} "$out/softhsm2.conf"
      '';
}
