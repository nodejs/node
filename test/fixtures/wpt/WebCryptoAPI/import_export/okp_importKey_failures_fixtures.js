// Setup: define the correct behaviors that should be sought, and create
// helper functions that generate all possible test parameters for
// different situations.
function getValidKeyData(algorithm) {
    return validKeyData[algorithm.name];
}

function getBadKeyLengthData(algorithm) {
    return badKeyLengthData[algorithm.name];
}

function getMissingJWKFieldKeyData(algorithm) {
    return missingJWKFieldKeyData[algorithm.name];
}

function getMismatchedJWKKeyData(algorithm) {
    return mismatchedJWKKeyData[algorithm.name];
}

var validKeyData = {
    "Ed25519": [
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
        }
    ],
    "Ed448": [
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
    ],
    "X25519": [
        {
            format: "spki",
            data: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 110, 3, 33, 0, 28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151, 6]),
        },
        {
            format: "pkcs8",
            data: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 110, 4, 34, 4, 32, 200, 131, 142, 118, 208, 87, 223, 183, 216, 201, 90, 105, 225, 56, 22, 10, 221, 99, 115, 253, 113, 164, 210, 118, 187, 86, 227, 168, 27, 100, 255, 97]),
        },
        {
            format: "raw",
            data: new Uint8Array([28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151, 6]),
        },
        {
            format: "jwk",
            data: {
                crv: "X25519",
                d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2E",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
                kty: "OKP"
            },
        },
        {
            format: "jwk",
            data: {
                crv: "X25519",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
                kty: "OKP"
            },
        },
    ],
    "X448": [
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
    ],
};

// Removed just the last byte.
var badKeyLengthData = {
    "Ed25519": [
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
        }
    ],
    "Ed448": [
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
    ],
    "X25519": [
        {
            format: "spki",
            data: new Uint8Array([48, 42, 48, 5, 6, 3, 43, 101, 110, 3, 33, 0, 28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151]),
        },
        {
            format: "pkcs8",
            data: new Uint8Array([48, 46, 2, 1, 0, 48, 5, 6, 3, 43, 101, 110, 4, 34, 4, 32, 200, 131, 142, 118, 208, 87, 223, 183, 216, 201, 90, 105, 225, 56, 22, 10, 221, 99, 115, 253, 113, 164, 210, 118, 187, 86, 227, 168, 27, 100, 255]),
        },
        {
            format: "raw",
            data: new Uint8Array([28, 242, 177, 230, 2, 46, 197, 55, 55, 30, 215, 245, 62, 84, 250, 17, 84, 216, 62, 152, 235, 100, 234, 81, 250, 229, 179, 48, 124, 254, 151]),
        },
        {
            format: "jwk",
            data: {
                crv: "X25519",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lw",
                kty: "OKP"
            }
        },
        {
            format: "jwk",
            data: {
                crv: "X25519",
                d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
                kty: "OKP"
            },
        },
    ],
    "X448": [
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
    ],
};

var missingJWKFieldKeyData =  {
    "Ed25519": [
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
    ],
    "Ed448": [
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
    ],
    "X25519": [
        {
            param: "x",
            data: {
                crv: "X25519",
                d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2E",
                kty: "OKP"
            },
        },
        {
            param: "kty",
            data: {
                crv: "X25519",
                d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2E",
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
            },
        },
        {
            param: "crv",
            data: {
                x: "HPKx5gIuxTc3Htf1PlT6EVTYPpjrZOpR-uWzMHz-lwY",
                kty: "OKP"
            },
        }
    ],
    "X448": [
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
    ],
};

// The public key doesn't match the private key.
var mismatchedJWKKeyData =  {
    "Ed25519": [
        {
            crv: "Ed25519",
            d: "88j0xI34eBRujNO_bfTlDjibpwdOFcI1Lc1dMI1MqB8",
            x: "11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo",
            kty: "OKP"
        },
    ],
    "Ed448": [
        {
            crv: "Ed448",
            d: "Dv8DRYwo4BecUh3jEslpt4NDSOyrmRpg47Lpp55M2eSA7ykXEtLIPQRyctXJ9ChmT2ltJnBFjx0u",
            x: "X9dEm1m0Yf0s54fsYWrUah2hNCSFpw4fig6nXYDpZ3jt8SR2m0bHBhvWeD3x5Q9s0foavq_oJWGA",
            kty: "OKP"
        },
    ],
    "X25519": [
        {
            crv: "X25519",
            d: "yIOOdtBX37fYyVpp4TgWCt1jc_1xpNJ2u1bjqBtk_2E",
            x: "hSDwCYkwp1R0i33ctD73Wg2_Og0mOBr066SpjqqbTmo",
            kty: "OKP"
        },
    ],
    "X448": [
        {

            crv: "X448",
            kty: "OKP",
            d: "WMfSmj61GbKdAM-xkbtk_G2KQtjxcXYnK4nyJy0YGSlcZSXAgpZxsFLvBydTDxiOMdDMU78mkp4",
            x: "mwj3zDG34+Z9ItWuoSEHSic70rg94Jxj+qc9LCLF2bvINmRyQdlT1AxbEtqIEg1TF3+A5TLEH6A",
        },
    ],
}
