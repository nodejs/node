// META: title=WebCryptoAPI: importKey() for Failures
// META: timeout=long
// META: script=../util/helpers.js
// META: script=okp_importKey_failures.js

// Setup: define the correct behaviors that should be sought, and create
// helper functions that generate all possible test parameters for
// different situations.
var validKeyData = [
    {
      format: "spki",
      data: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 112, 3, 33, 0, 216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204])
    },
    {
      format: "pkcs8",
      data: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 112, 4, 34, 4, 32, 243, 200, 244, 196, 141, 248, 120, 20, 110, 140, 211, 191, 109, 244, 229, 14, 56, 155, 167, 7, 78, 21, 194, 53, 45, 205, 93, 48, 141, 76, 168, 31])
    },
    {
      format: "raw",
      data: new Uint8Array([216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61, 204])
    },
    {
      format: "jwk",
      data: {
          crv: "Ed25519",
          d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
          x: "2OGJY9gJ1IfZVJrMrsZ0Ln7rok2KDTsUt-PK6gaJPcw",
          kty: "OKP"
      },
    },
    {
      format: "jwk",
      data: {
          crv: "Ed25519",
          x: "2OGJY9gJ1IfZVJrMrsZ0Ln7rok2KDTsUt-PK6gaJPcw",
          kty: "OKP"
      },
    },
];

// Removed just the last byte.
var badKeyLengthData = [
    {
        format: "spki",
        data: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 112, 3, 33, 0, 216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61])
    },
    {
        format: "pkcs8",
        data: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 112, 4, 34, 4, 32, 243, 200, 244, 196, 141, 248, 120, 20, 110, 140, 211, 191, 109, 244, 229, 14, 56, 155, 167, 7, 78, 21, 194, 53, 45, 205, 93, 48, 141, 76, 168])
    },
    {
        format: "raw",
        data: new Uint8Array([216, 225, 137, 99, 216, 9, 212, 135, 217, 84, 154, 204, 174, 198, 116, 46, 126, 235, 162, 77, 138, 13, 59, 20, 183, 227, 202, 234, 6, 137, 61])
    },
    {
        format: "jwk",
        data: {
            crv: "Ed25519",
            d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB",
            x: "2OGJY9gJ1IfZVJrMrsZ0Ln7rok2KDTsUt-PK6gaJPcw",
            kty: "OKP"
        }
    },
    {
        format: "jwk",
        data: {
            crv: "Ed25519",
            x: "2OGJY9gJ1IfZVJrMrsZ0Ln7rok2KDTsUt-PK6gaJPc",
            kty: "OKP"
        }
    },
];

var missingJWKFieldKeyData = [
    {
        param: "x",
        data: {
            crv: "Ed25519",
            d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
            kty: "OKP"
        },
    },
    {
        param: "kty",
        data: {
            crv: "Ed25519",
            x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
            d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
        },
    },
    {
        param: "crv",
        data: {
            x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
            kty: "OKP"
        },
    }
];

// The public key doesn't match the private key.
var invalidJWKKeyData = [
    {
        crv: "Ed25519",
        d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
        x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
        kty: "OKP"
    },
];

run_test(["Ed25519"]);
