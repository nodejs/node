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
        data: new Uint8Array([48, 66, 48, 5, 6, 3, 43, 101, 111, 3, 57, 0, 182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206, 111]),
    },
    {
        format: "pkcs8",
        data: new Uint8Array([48, 70, 2, 1, 0, 48, 5, 6, 3, 43, 101, 111, 4, 58, 4, 56, 88, 199, 210, 154, 62, 181, 25, 178, 157, 0, 207, 177, 145, 187, 100, 252, 109, 138, 66, 216, 241, 113, 118, 39, 43, 137, 242, 39, 45, 24, 25, 41, 92, 101, 37, 192, 130, 150, 113, 176, 82, 239, 7, 39, 83, 15, 24, 142, 49, 208, 204, 83, 191, 38, 146, 158]),
    },
    {
        format: "raw",
        data: new Uint8Array([182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206, 111]),
    },
    {
        format: "jwk",
        data: {
            crv: "X448",
            d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
            kty: "OKP"
        },
    },
    {
        format: "jwk",
        data: {
          crv: "X448",
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
            kty: "OKP"
        },
    },
];

// Removed just the last byte.
var badKeyLengthData = [
    {
        format: "spki",
        data: new Uint8Array([48, 66, 48, 5, 6, 3, 43, 101, 111, 3, 57, 0, 182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206]),
    },
    {
        format: "pkcs8",
        data: new Uint8Array([48, 70, 2, 1, 0, 48, 5, 6, 3, 43, 101, 111, 4, 58, 4, 56, 88, 199, 210, 154, 62, 181, 25, 178, 157, 0, 207, 177, 145, 187, 100, 252, 109, 138, 66, 216, 241, 113, 118, 39, 43, 137, 242, 39, 45, 24, 25, 41, 92, 101, 37, 192, 130, 150, 113, 176, 82, 239, 7, 39, 83, 15, 24, 142, 49, 208, 204, 83, 191, 38, 146]),
    },
    {
        format: "raw",
        data: new Uint8Array([182, 4, 161, 209, 165, 205, 29, 148, 38, 213, 97, 239, 99, 10, 158, 177, 108, 190, 105, 213, 185, 202, 97, 94, 220, 83, 99, 62, 251, 82, 234, 49, 230, 230, 160, 161, 219, 172, 198, 231, 108, 188, 230, 72, 45, 126, 75, 163, 213, 93, 158, 128, 39, 101, 206]),
    },
    {
        format: "jwk",
        data: {
            crv: "X448",
            d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp",
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
            kty: "OKP"
        },
    },
    {
        format: "jwk",
        data: {
            crv: "X448",
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm",
            kty: "OKP"
        },
    },
];

var missingJWKFieldKeyData = [
    {
        param: "x",
        data: {
            crv: "X448",
            d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
            kty: "OKP"
        }
    },
    {
        param: "kty",
        data: {
            crv: "X448",
            d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
        }
    },
    {
        param: "crv",
        data: {
            x: "tgSh0aXNHZQm1WHvYwqesWy-adW5ymFe3FNjPvtS6jHm5qCh26zG52y85kgtfkuj1V2egCdlzm8",
            kty: "OKP"
        }
    }
];

// The public key doesn't match the private key.
var invalidJWKKeyData = [
    {

        crv: "X448",
        kty: "OKP",
        d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
        x: "mwj3zDG34+Z9ItWuoSEHSic70rg94Jxj+qc9LCLF2bvINmRyQdlT1AxbEtqIEg1TF3+A5TLEH6A",
    },
];

run_test(["X448"]);
