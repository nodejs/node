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
        data: new Uint8Array([48, 67, 48, 5, 6, 3, 43, 101, 113, 3, 58, 0, 171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90, 128]),
    },
    {
        format: "pkcs8",
        data: new Uint8Array([48, 71, 2, 1, 0, 48, 5, 6, 3, 43, 101, 113, 4, 59, 4, 57, 14, 255, 3, 69, 140, 40, 224, 23, 156, 82, 29, 227, 18, 201, 105, 183, 131, 67, 72, 236, 171, 153, 26, 96, 227, 178, 233, 167, 158, 76, 217, 228, 128, 239, 41, 23, 18, 210, 200, 61, 4, 114, 114, 213, 201, 244, 40, 102, 79, 105, 109, 38, 112, 69, 143, 29, 46]),
    },
    {
        format: "raw",
        data: new Uint8Array([171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90, 128]),
    },
    {
        format: "jwk",
        data: {
            crv: "Ed448",
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
            kty: "OKP"
        },
    },
    {
        format: "jwk",
        data: {
          crv: "Ed448",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
            kty: "OKP"
        },
    },
];

// Removed just the last byte.
var badKeyLengthData = [
    {
        format: "spki",
        data: new Uint8Array([48, 67, 48, 5, 6, 3, 43, 101, 113, 3, 58, 0, 171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90]),
    },
    {
        format: "pkcs8",
        data: new Uint8Array([48, 71, 2, 1, 0, 48, 5, 6, 3, 43, 101, 113, 4, 59, 4, 57, 14, 255, 3, 69, 140, 40, 224, 23, 156, 82, 29, 227, 18, 201, 105, 183, 131, 67, 72, 236, 171, 153, 26, 96, 227, 178, 233, 167, 158, 76, 217, 228, 128, 239, 41, 23, 18, 210, 200, 61, 4, 114, 114, 213, 201, 244, 40, 102, 79, 105, 109, 38, 112, 69, 143, 29]),
    },
    {
        format: "raw",
        data: new Uint8Array([171, 75, 184, 133, 253, 125, 44, 90, 242, 78, 131, 113, 12, 255, 160, 199, 74, 87, 226, 116, 128, 29, 178, 5, 123, 11, 220, 94, 160, 50, 182, 254, 107, 199, 139, 128, 69, 54, 90, 235, 38, 232, 110, 31, 20, 253, 52, 157, 7, 196, 132, 149, 245, 164, 106, 90]),
    },
    {
        format: "jwk",
        data: {
            crv: "Ed448",
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
            kty: "OKP"
        },
    },
    {
        format: "jwk",
        data: {
          crv: "Ed448",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalq",
            kty: "OKP"
        },
    },
];

var missingJWKFieldKeyData = [
    {
        param: "x",
        data: {
            crv: "Ed448",
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
            kty: "OKP"
        }
    },
    {
        param: "kty",
        data: {
            crv: "Ed448",
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
        }
    },
    {
        param: "crv",
        data: {
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
            x: "q0u4hf19LFryToNxDP-gx0pX4nSAHbIFewvcXqAytv5rx4uARTZa6ybobh8U_TSdB8SElfWkalqA",
            kty: "OKP"
        }
    }
];

// The public key doesn't match the private key.
var invalidJWKKeyData = [
    {
        crv: "Ed448",
        d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
        x: "X9dEm1m0Yf0s54fsYWrUah2hNCSFpw4fig6nXYDpZ3jt8SR2m0bHBhvWeD3x5Q9s0foavq_oJWGA",
        kty: "OKP"
    },
];

run_test(["Ed448"]);
