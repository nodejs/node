// META: title=WebCryptoAPI: importKey() for RSA keys
// META: timeout=long
// META: script=../util/helpers.js

// Test importKey and exportKey for RSA algorithms. Only "happy paths" are
// currently tested - those where the operation should succeed.

    var subtle = crypto.subtle;

    var sizes = [1024, 2048, 4096];

    var hashes = ["SHA-1", "SHA-256", "SHA-384", "SHA-512"];

    var keyData = {
        1024: {
            spki: new Uint8Array([48, 129, 159, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 3, 129, 141, 0, 48, 129, 137, 2, 129, 129, 0, 205, 153, 248, 177, 17, 159, 141, 10, 44, 231, 172, 139, 253, 12, 181, 71, 211, 72, 249, 49, 204, 156, 92, 167, 159, 222, 32, 229, 28, 64, 235, 1, 171, 38, 30, 1, 37, 61, 241, 232, 143, 113, 208, 134, 233, 75, 122, 190, 119, 131, 145, 3, 164, 118, 190, 224, 204, 135, 199, 67, 21, 26, 253, 68, 49, 250, 93, 143, 160, 81, 39, 28, 245, 78, 73, 207, 117, 0, 216, 169, 149, 126, 192, 155, 157, 67, 239, 112, 9, 140, 87, 241, 13, 3, 191, 211, 23, 72, 175, 86, 59, 136, 22, 135, 114, 13, 60, 123, 16, 161, 205, 85, 58, 199, 29, 41, 107, 110, 222, 236, 165, 185, 156, 138, 251, 54, 221, 151, 2, 3, 1, 0, 1]),
            pkcs8: new Uint8Array([48, 130, 2, 120, 2, 1, 0, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 4, 130, 2, 98, 48, 130, 2, 94, 2, 1, 0, 2, 129, 129, 0, 205, 153, 248, 177, 17, 159, 141, 10, 44, 231, 172, 139, 253, 12, 181, 71, 211, 72, 249, 49, 204, 156, 92, 167, 159, 222, 32, 229, 28, 64, 235, 1, 171, 38, 30, 1, 37, 61, 241, 232, 143, 113, 208, 134, 233, 75, 122, 190, 119, 131, 145, 3, 164, 118, 190, 224, 204, 135, 199, 67, 21, 26, 253, 68, 49, 250, 93, 143, 160, 81, 39, 28, 245, 78, 73, 207, 117, 0, 216, 169, 149, 126, 192, 155, 157, 67, 239, 112, 9, 140, 87, 241, 13, 3, 191, 211, 23, 72, 175, 86, 59, 136, 22, 135, 114, 13, 60, 123, 16, 161, 205, 85, 58, 199, 29, 41, 107, 110, 222, 236, 165, 185, 156, 138, 251, 54, 221, 151, 2, 3, 1, 0, 1, 2, 129, 128, 98, 162, 10, 252, 103, 71, 243, 145, 126, 25, 102, 93, 129, 248, 38, 191, 94, 77, 19, 191, 32, 57, 162, 249, 135, 104, 56, 191, 176, 222, 51, 223, 137, 11, 176, 57, 60, 116, 139, 40, 214, 39, 243, 177, 197, 25, 192, 184, 190, 253, 15, 4, 128, 81, 183, 32, 128, 254, 98, 73, 124, 70, 134, 88, 228, 85, 8, 229, 210, 6, 149, 141, 122, 147, 24, 166, 42, 57, 218, 125, 240, 230, 232, 249, 81, 145, 44, 6, 118, 237, 101, 205, 4, 181, 104, 85, 23, 96, 46, 169, 174, 213, 110, 34, 171, 89, 196, 20, 18, 1, 8, 241, 93, 32, 19, 144, 248, 183, 32, 96, 240, 101, 239, 247, 222, 249, 117, 1, 2, 65, 0, 244, 26, 192, 131, 146, 245, 205, 250, 134, 62, 229, 137, 14, 224, 194, 5, 127, 147, 154, 214, 93, 172, 226, 55, 98, 206, 25, 104, 223, 178, 48, 249, 83, 143, 5, 146, 16, 243, 180, 170, 119, 227, 17, 151, 48, 217, 88, 23, 30, 2, 73, 153, 181, 92, 163, 164, 241, 114, 66, 66, 152, 70, 42, 121, 2, 65, 0, 215, 158, 227, 12, 157, 88, 107, 153, 230, 66, 244, 207, 110, 18, 128, 60, 7, 140, 90, 136, 49, 11, 38, 144, 78, 64, 107, 167, 125, 41, 16, 167, 122, 152, 100, 129, 223, 206, 97, 170, 190, 1, 34, 79, 44, 221, 254, 204, 117, 122, 76, 249, 68, 169, 105, 152, 20, 161, 62, 40, 255, 101, 68, 143, 2, 65, 0, 169, 215, 127, 65, 76, 220, 104, 31, 186, 142, 66, 168, 213, 72, 62, 215, 18, 136, 2, 0, 203, 22, 194, 35, 37, 69, 31, 90, 223, 226, 28, 191, 45, 139, 98, 165, 217, 211, 167, 77, 192, 178, 166, 7, 155, 62, 110, 83, 79, 86, 234, 28, 223, 154, 128, 102, 0, 116, 174, 115, 165, 125, 148, 137, 2, 65, 0, 132, 212, 95, 192, 228, 169, 148, 215, 225, 46, 252, 75, 80, 222, 218, 218, 160, 55, 201, 137, 190, 212, 196, 179, 255, 80, 214, 64, 254, 236, 174, 82, 206, 70, 85, 28, 96, 248, 109, 216, 86, 102, 178, 113, 30, 13, 192, 42, 202, 112, 70, 61, 5, 28, 108, 109, 128, 191, 248, 96, 31, 61, 142, 103, 2, 65, 0, 205, 186, 73, 64, 8, 98, 158, 188, 82, 109, 82, 177, 5, 13, 132, 100, 97, 84, 15, 103, 183, 88, 37, 219, 0, 148, 88, 166, 79, 7, 85, 14, 64, 3, 157, 142, 132, 164, 226, 112, 236, 158, 218, 17, 7, 158, 184, 41, 20, 172, 194, 242, 44, 231, 78, 192, 134, 220, 83, 36, 191, 7, 35, 225]),
            jwk: {
                kty: "RSA",
                n: "zZn4sRGfjQos56yL_Qy1R9NI-THMnFynn94g5RxA6wGrJh4BJT3x6I9x0IbpS3q-d4ORA6R2vuDMh8dDFRr9RDH6XY-gUScc9U5Jz3UA2KmVfsCbnUPvcAmMV_ENA7_TF0ivVjuIFodyDTx7EKHNVTrHHSlrbt7spbmcivs23Zc",
                e: "AQAB",
                d: "YqIK_GdH85F-GWZdgfgmv15NE78gOaL5h2g4v7DeM9-JC7A5PHSLKNYn87HFGcC4vv0PBIBRtyCA_mJJfEaGWORVCOXSBpWNepMYpio52n3w5uj5UZEsBnbtZc0EtWhVF2Auqa7VbiKrWcQUEgEI8V0gE5D4tyBg8GXv9975dQE",
                p: "9BrAg5L1zfqGPuWJDuDCBX-TmtZdrOI3Ys4ZaN-yMPlTjwWSEPO0qnfjEZcw2VgXHgJJmbVco6TxckJCmEYqeQ",
                q: "157jDJ1Ya5nmQvTPbhKAPAeMWogxCyaQTkBrp30pEKd6mGSB385hqr4BIk8s3f7MdXpM-USpaZgUoT4o_2VEjw",
                dp: "qdd_QUzcaB-6jkKo1Ug-1xKIAgDLFsIjJUUfWt_iHL8ti2Kl2dOnTcCypgebPm5TT1bqHN-agGYAdK5zpX2UiQ",
                dq: "hNRfwOSplNfhLvxLUN7a2qA3yYm-1MSz_1DWQP7srlLORlUcYPht2FZmsnEeDcAqynBGPQUcbG2Av_hgHz2OZw",
                qi: "zbpJQAhinrxSbVKxBQ2EZGFUD2e3WCXbAJRYpk8HVQ5AA52OhKTicOye2hEHnrgpFKzC8iznTsCG3FMkvwcj4Q"
            }
        },

        2048: {
            spki: new Uint8Array([48, 130, 1, 34, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 3, 130, 1, 15, 0, 48, 130, 1, 10, 2, 130, 1, 1, 0, 217, 133, 128, 235, 45, 23, 114, 244, 164, 118, 188, 84, 4, 190, 230, 13, 154, 60, 42, 203, 188, 242, 74, 116, 117, 77, 159, 90, 104, 18, 56, 143, 158, 63, 38, 10, 216, 22, 135, 221, 179, 102, 248, 218, 85, 148, 98, 179, 151, 241, 192, 151, 137, 109, 13, 246, 230, 222, 49, 192, 79, 141, 71, 205, 21, 96, 13, 17, 190, 78, 196, 230, 48, 158, 32, 4, 22, 37, 127, 171, 186, 139, 190, 211, 58, 176, 193, 101, 218, 60, 155, 31, 206, 194, 196, 233, 229, 42, 202, 99, 89, 167, 207, 84, 213, 39, 91, 68, 134, 191, 1, 162, 180, 95, 4, 250, 226, 11, 113, 125, 1, 167, 148, 87, 7, 40, 129, 82, 151, 178, 183, 242, 43, 224, 14, 243, 2, 56, 19, 202, 135, 183, 224, 190, 131, 67, 51, 92, 250, 240, 118, 158, 54, 108, 249, 37, 108, 244, 66, 57, 69, 139, 180, 126, 189, 107, 50, 240, 22, 137, 128, 103, 0, 146, 115, 247, 157, 69, 184, 91, 159, 51, 245, 115, 24, 223, 197, 175, 152, 26, 162, 150, 72, 52, 231, 245, 179, 48, 18, 211, 105, 100, 106, 103, 56, 178, 43, 202, 85, 229, 144, 102, 241, 230, 159, 106, 105, 241, 238, 222, 204, 232, 129, 183, 66, 63, 212, 77, 252, 122, 124, 152, 156, 66, 103, 65, 216, 129, 60, 63, 205, 192, 36, 181, 61, 132, 41, 10, 59, 237, 163, 200, 56, 114, 202, 253, 2, 3, 1, 0, 1]),
            pkcs8: new Uint8Array([48, 130, 4, 190, 2, 1, 0, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 4, 130, 4, 168, 48, 130, 4, 164, 2, 1, 0, 2, 130, 1, 1, 0, 217, 133, 128, 235, 45, 23, 114, 244, 164, 118, 188, 84, 4, 190, 230, 13, 154, 60, 42, 203, 188, 242, 74, 116, 117, 77, 159, 90, 104, 18, 56, 143, 158, 63, 38, 10, 216, 22, 135, 221, 179, 102, 248, 218, 85, 148, 98, 179, 151, 241, 192, 151, 137, 109, 13, 246, 230, 222, 49, 192, 79, 141, 71, 205, 21, 96, 13, 17, 190, 78, 196, 230, 48, 158, 32, 4, 22, 37, 127, 171, 186, 139, 190, 211, 58, 176, 193, 101, 218, 60, 155, 31, 206, 194, 196, 233, 229, 42, 202, 99, 89, 167, 207, 84, 213, 39, 91, 68, 134, 191, 1, 162, 180, 95, 4, 250, 226, 11, 113, 125, 1, 167, 148, 87, 7, 40, 129, 82, 151, 178, 183, 242, 43, 224, 14, 243, 2, 56, 19, 202, 135, 183, 224, 190, 131, 67, 51, 92, 250, 240, 118, 158, 54, 108, 249, 37, 108, 244, 66, 57, 69, 139, 180, 126, 189, 107, 50, 240, 22, 137, 128, 103, 0, 146, 115, 247, 157, 69, 184, 91, 159, 51, 245, 115, 24, 223, 197, 175, 152, 26, 162, 150, 72, 52, 231, 245, 179, 48, 18, 211, 105, 100, 106, 103, 56, 178, 43, 202, 85, 229, 144, 102, 241, 230, 159, 106, 105, 241, 238, 222, 204, 232, 129, 183, 66, 63, 212, 77, 252, 122, 124, 152, 156, 66, 103, 65, 216, 129, 60, 63, 205, 192, 36, 181, 61, 132, 41, 10, 59, 237, 163, 200, 56, 114, 202, 253, 2, 3, 1, 0, 1, 2, 130, 1, 0, 90, 210, 167, 117, 138, 170, 83, 209, 90, 42, 73, 144, 59, 59, 10, 11, 123, 238, 203, 95, 174, 80, 236, 77, 155, 253, 1, 32, 90, 123, 225, 41, 246, 69, 31, 185, 63, 104, 136, 234, 68, 210, 37, 237, 227, 245, 197, 16, 127, 204, 237, 65, 88, 156, 52, 76, 119, 49, 39, 76, 200, 234, 144, 164, 76, 220, 130, 24, 122, 129, 161, 45, 11, 247, 186, 30, 122, 176, 197, 146, 10, 157, 246, 219, 115, 146, 1, 238, 105, 37, 13, 16, 70, 224, 132, 31, 181, 20, 28, 213, 70, 198, 14, 135, 185, 72, 105, 143, 63, 67, 217, 134, 250, 17, 2, 159, 78, 106, 192, 196, 21, 64, 199, 107, 95, 13, 198, 144, 212, 69, 255, 226, 191, 121, 46, 30, 103, 153, 111, 171, 166, 137, 88, 229, 86, 142, 66, 238, 136, 24, 72, 248, 27, 43, 116, 101, 215, 99, 39, 246, 212, 111, 241, 132, 169, 7, 252, 19, 104, 172, 233, 8, 40, 227, 172, 42, 47, 36, 134, 34, 214, 97, 228, 179, 215, 193, 4, 222, 129, 165, 1, 59, 216, 171, 50, 17, 100, 68, 199, 226, 114, 175, 49, 6, 95, 129, 122, 189, 198, 152, 17, 113, 70, 121, 104, 51, 75, 18, 210, 27, 237, 93, 87, 104, 49, 64, 112, 122, 198, 34, 61, 209, 7, 6, 121, 22, 191, 95, 151, 248, 124, 7, 87, 143, 45, 123, 22, 128, 153, 197, 130, 196, 244, 164, 225, 241, 2, 129, 129, 0, 252, 223, 109, 18, 211, 223, 124, 146, 67, 138, 211, 142, 156, 153, 102, 192, 192, 236, 129, 21, 14, 158, 28, 228, 12, 184, 69, 239, 165, 195, 209, 9, 236, 240, 88, 59, 143, 104, 199, 197, 124, 83, 168, 201, 166, 249, 158, 156, 67, 158, 15, 116, 155, 224, 83, 172, 112, 187, 1, 225, 127, 254, 175, 175, 214, 214, 36, 111, 218, 85, 109, 33, 228, 157, 192, 61, 195, 207, 25, 136, 154, 244, 134, 69, 18, 103, 225, 172, 131, 16, 168, 70, 3, 30, 5, 98, 162, 47, 88, 191, 99, 241, 127, 93, 36, 4, 72, 97, 227, 7, 70, 60, 141, 25, 150, 77, 170, 201, 86, 129, 29, 96, 60, 41, 231, 190, 200, 107, 2, 129, 129, 0, 220, 54, 40, 140, 204, 79, 7, 149, 241, 40, 229, 237, 13, 3, 118, 172, 76, 61, 137, 8, 253, 72, 223, 119, 189, 19, 87, 199, 3, 61, 197, 45, 111, 18, 58, 224, 121, 190, 144, 46, 143, 225, 7, 129, 10, 154, 24, 140, 96, 246, 212, 224, 232, 144, 67, 98, 6, 188, 167, 17, 224, 215, 160, 182, 249, 132, 174, 249, 21, 78, 138, 59, 186, 184, 239, 10, 71, 146, 46, 189, 206, 165, 57, 50, 38, 241, 230, 57, 169, 77, 76, 229, 53, 45, 184, 87, 22, 194, 94, 48, 68, 246, 171, 255, 73, 197, 25, 64, 13, 132, 56, 120, 241, 100, 197, 243, 171, 84, 246, 32, 86, 55, 55, 216, 121, 64, 52, 55, 2, 129, 128, 109, 221, 189, 12, 35, 21, 196, 143, 223, 220, 159, 82, 36, 227, 217, 107, 1, 231, 63, 166, 32, 117, 189, 227, 175, 75, 24, 199, 168, 99, 205, 156, 220, 95, 8, 86, 200, 86, 36, 5, 191, 160, 177, 130, 251, 147, 20, 192, 155, 248, 62, 138, 209, 118, 195, 163, 246, 78, 169, 224, 137, 181, 228, 43, 39, 210, 94, 126, 98, 132, 31, 40, 76, 165, 229, 114, 112, 114, 184, 139, 75, 151, 214, 6, 136, 154, 173, 200, 64, 33, 170, 154, 208, 155, 232, 135, 20, 36, 50, 16, 229, 161, 117, 78, 200, 105, 59, 241, 155, 171, 251, 110, 47, 119, 224, 127, 218, 38, 35, 249, 113, 3, 240, 223, 220, 26, 94, 5, 2, 129, 129, 0, 149, 113, 187, 187, 49, 188, 64, 109, 165, 168, 23, 193, 244, 30, 241, 158, 164, 110, 238, 92, 199, 103, 121, 32, 141, 148, 94, 241, 148, 101, 139, 54, 246, 53, 236, 247, 2, 40, 45, 57, 44, 51, 143, 32, 39, 205, 195, 243, 32, 170, 226, 117, 111, 222, 215, 155, 226, 238, 140, 131, 57, 143, 156, 102, 16, 151, 215, 22, 251, 58, 189, 221, 35, 46, 246, 42, 135, 191, 209, 48, 198, 216, 162, 36, 67, 1, 207, 56, 58, 137, 87, 50, 6, 16, 237, 21, 77, 64, 195, 35, 6, 234, 80, 119, 131, 220, 218, 241, 249, 58, 78, 8, 229, 233, 121, 221, 143, 220, 172, 219, 237, 38, 180, 35, 152, 197, 213, 169, 2, 129, 129, 0, 157, 34, 27, 203, 101, 161, 91, 231, 149, 223, 255, 186, 178, 175, 168, 93, 194, 163, 171, 101, 186, 95, 110, 38, 250, 23, 38, 18, 213, 87, 33, 41, 187, 18, 0, 21, 202, 68, 70, 236, 63, 219, 158, 201, 128, 166, 97, 210, 170, 210, 56, 80, 81, 24, 152, 240, 124, 20, 135, 22, 9, 92, 209, 189, 96, 214, 49, 70, 74, 200, 155, 82, 70, 96, 189, 70, 89, 82, 210, 229, 125, 135, 64, 183, 195, 243, 219, 121, 73, 43, 22, 184, 122, 92, 209, 118, 126, 19, 82, 110, 246, 109, 121, 198, 145, 226, 199, 242, 82, 139, 105, 101, 44, 41, 186, 33, 10, 94, 103, 157, 35, 178, 26, 104, 12, 191, 13, 7]),
            jwk: {
                kty: "RSA",
                n: "2YWA6y0XcvSkdrxUBL7mDZo8Ksu88kp0dU2fWmgSOI-ePyYK2BaH3bNm-NpVlGKzl_HAl4ltDfbm3jHAT41HzRVgDRG-TsTmMJ4gBBYlf6u6i77TOrDBZdo8mx_OwsTp5SrKY1mnz1TVJ1tEhr8BorRfBPriC3F9AaeUVwcogVKXsrfyK-AO8wI4E8qHt-C-g0MzXPrwdp42bPklbPRCOUWLtH69azLwFomAZwCSc_edRbhbnzP1cxjfxa-YGqKWSDTn9bMwEtNpZGpnOLIrylXlkGbx5p9qafHu3szogbdCP9RN_Hp8mJxCZ0HYgTw_zcAktT2EKQo77aPIOHLK_Q",
                e: "AQAB",
                d: "WtKndYqqU9FaKkmQOzsKC3vuy1-uUOxNm_0BIFp74Sn2RR-5P2iI6kTSJe3j9cUQf8ztQVicNEx3MSdMyOqQpEzcghh6gaEtC_e6HnqwxZIKnfbbc5IB7mklDRBG4IQftRQc1UbGDoe5SGmPP0PZhvoRAp9OasDEFUDHa18NxpDURf_iv3kuHmeZb6umiVjlVo5C7ogYSPgbK3Rl12Mn9tRv8YSpB_wTaKzpCCjjrCovJIYi1mHks9fBBN6BpQE72KsyEWREx-JyrzEGX4F6vcaYEXFGeWgzSxLSG-1dV2gxQHB6xiI90QcGeRa_X5f4fAdXjy17FoCZxYLE9KTh8Q",
                p: "_N9tEtPffJJDitOOnJlmwMDsgRUOnhzkDLhF76XD0Qns8Fg7j2jHxXxTqMmm-Z6cQ54PdJvgU6xwuwHhf_6vr9bWJG_aVW0h5J3APcPPGYia9IZFEmfhrIMQqEYDHgVioi9Yv2Pxf10kBEhh4wdGPI0Zlk2qyVaBHWA8Kee-yGs",
                q: "3DYojMxPB5XxKOXtDQN2rEw9iQj9SN93vRNXxwM9xS1vEjrgeb6QLo_hB4EKmhiMYPbU4OiQQ2IGvKcR4NegtvmErvkVToo7urjvCkeSLr3OpTkyJvHmOalNTOU1LbhXFsJeMET2q_9JxRlADYQ4ePFkxfOrVPYgVjc32HlANDc",
                dp: "bd29DCMVxI_f3J9SJOPZawHnP6Ygdb3jr0sYx6hjzZzcXwhWyFYkBb-gsYL7kxTAm_g-itF2w6P2TqngibXkKyfSXn5ihB8oTKXlcnByuItLl9YGiJqtyEAhqprQm-iHFCQyEOWhdU7IaTvxm6v7bi934H_aJiP5cQPw39waXgU",
                dq: "lXG7uzG8QG2lqBfB9B7xnqRu7lzHZ3kgjZRe8ZRlizb2Nez3AigtOSwzjyAnzcPzIKridW_e15vi7oyDOY-cZhCX1xb7Or3dIy72Koe_0TDG2KIkQwHPODqJVzIGEO0VTUDDIwbqUHeD3Nrx-TpOCOXped2P3Kzb7Sa0I5jF1ak",
                qi: "nSIby2WhW-eV3_-6sq-oXcKjq2W6X24m-hcmEtVXISm7EgAVykRG7D_bnsmApmHSqtI4UFEYmPB8FIcWCVzRvWDWMUZKyJtSRmC9RllS0uV9h0C3w_PbeUkrFrh6XNF2fhNSbvZtecaR4sfyUotpZSwpuiEKXmedI7IaaAy_DQc"
            }
        },

        4096: {
            spki: new Uint8Array([48, 130, 2, 34, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 3, 130, 2, 15, 0, 48, 130, 2, 10, 2, 130, 2, 1, 0, 218, 170, 246, 76, 189, 156, 216, 153, 155, 176, 221, 14, 44, 132, 103, 104, 0, 127, 100, 166, 245, 248, 104, 125, 31, 74, 155, 226, 90, 193, 184, 54, 170, 145, 111, 222, 20, 252, 19, 248, 146, 44, 190, 115, 73, 188, 52, 251, 4, 178, 121, 238, 212, 204, 34, 62, 122, 100, 203, 111, 233, 231, 210, 73, 53, 146, 147, 211, 14, 161, 109, 137, 212, 175, 226, 18, 183, 173, 103, 103, 30, 128, 31, 218, 69, 126, 234, 65, 88, 231, 160, 91, 51, 245, 77, 54, 4, 167, 192, 33, 68, 244, 163, 242, 187, 111, 209, 180, 241, 221, 107, 172, 5, 40, 134, 47, 210, 85, 8, 112, 57, 186, 29, 131, 176, 93, 116, 198, 202, 82, 108, 251, 209, 3, 72, 75, 143, 59, 44, 222, 56, 89, 69, 103, 159, 211, 160, 19, 214, 173, 77, 133, 0, 68, 219, 164, 79, 64, 238, 65, 189, 201, 248, 173, 180, 146, 196, 238, 86, 232, 215, 109, 39, 165, 162, 16, 230, 46, 134, 234, 148, 106, 34, 230, 198, 63, 231, 143, 16, 179, 208, 109, 22, 100, 54, 156, 107, 132, 28, 208, 118, 205, 217, 89, 228, 75, 196, 169, 181, 5, 85, 157, 144, 110, 129, 186, 141, 119, 104, 162, 206, 170, 115, 7, 96, 82, 240, 33, 143, 81, 243, 215, 67, 96, 137, 207, 209, 22, 162, 251, 108, 208, 232, 32, 236, 205, 167, 174, 161, 116, 13, 249, 187, 22, 240, 185, 172, 160, 103, 94, 162, 147, 26, 15, 143, 183, 147, 98, 231, 117, 134, 185, 50, 64, 40, 30, 27, 13, 152, 132, 40, 138, 32, 78, 158, 162, 207, 212, 229, 210, 251, 88, 116, 67, 229, 164, 164, 147, 59, 32, 94, 217, 197, 242, 149, 102, 74, 219, 46, 127, 68, 28, 116, 10, 2, 249, 231, 130, 123, 29, 45, 73, 56, 17, 195, 208, 45, 25, 60, 252, 98, 189, 109, 25, 0, 253, 151, 254, 124, 211, 48, 23, 156, 78, 163, 154, 188, 17, 69, 14, 188, 16, 64, 59, 190, 136, 70, 162, 253, 237, 156, 111, 41, 27, 40, 63, 205, 204, 94, 0, 50, 237, 62, 87, 211, 115, 91, 68, 194, 104, 119, 72, 106, 226, 160, 48, 165, 138, 134, 2, 138, 153, 181, 38, 249, 48, 120, 72, 15, 245, 227, 15, 164, 64, 188, 74, 4, 84, 213, 83, 67, 73, 87, 181, 72, 94, 46, 54, 193, 252, 188, 14, 207, 28, 82, 159, 131, 168, 238, 168, 145, 28, 230, 27, 126, 151, 93, 5, 96, 68, 126, 66, 174, 155, 101, 123, 20, 218, 131, 92, 124, 78, 82, 44, 55, 139, 77, 105, 177, 136, 121, 177, 43, 77, 12, 240, 0, 76, 20, 133, 121, 129, 73, 15, 160, 200, 150, 114, 95, 59, 59, 165, 240, 204, 13, 156, 134, 194, 4, 70, 158, 213, 111, 229, 103, 216, 239, 132, 16, 184, 151, 206, 254, 229, 62, 23, 58, 125, 49, 144, 208, 215, 2, 3, 1, 0, 1]),
            pkcs8: new Uint8Array([48, 130, 9, 68, 2, 1, 0, 48, 13, 6, 9, 42, 134, 72, 134, 247, 13, 1, 1, 1, 5, 0, 4, 130, 9, 46, 48, 130, 9, 42, 2, 1, 0, 2, 130, 2, 1, 0, 218, 170, 246, 76, 189, 156, 216, 153, 155, 176, 221, 14, 44, 132, 103, 104, 0, 127, 100, 166, 245, 248, 104, 125, 31, 74, 155, 226, 90, 193, 184, 54, 170, 145, 111, 222, 20, 252, 19, 248, 146, 44, 190, 115, 73, 188, 52, 251, 4, 178, 121, 238, 212, 204, 34, 62, 122, 100, 203, 111, 233, 231, 210, 73, 53, 146, 147, 211, 14, 161, 109, 137, 212, 175, 226, 18, 183, 173, 103, 103, 30, 128, 31, 218, 69, 126, 234, 65, 88, 231, 160, 91, 51, 245, 77, 54, 4, 167, 192, 33, 68, 244, 163, 242, 187, 111, 209, 180, 241, 221, 107, 172, 5, 40, 134, 47, 210, 85, 8, 112, 57, 186, 29, 131, 176, 93, 116, 198, 202, 82, 108, 251, 209, 3, 72, 75, 143, 59, 44, 222, 56, 89, 69, 103, 159, 211, 160, 19, 214, 173, 77, 133, 0, 68, 219, 164, 79, 64, 238, 65, 189, 201, 248, 173, 180, 146, 196, 238, 86, 232, 215, 109, 39, 165, 162, 16, 230, 46, 134, 234, 148, 106, 34, 230, 198, 63, 231, 143, 16, 179, 208, 109, 22, 100, 54, 156, 107, 132, 28, 208, 118, 205, 217, 89, 228, 75, 196, 169, 181, 5, 85, 157, 144, 110, 129, 186, 141, 119, 104, 162, 206, 170, 115, 7, 96, 82, 240, 33, 143, 81, 243, 215, 67, 96, 137, 207, 209, 22, 162, 251, 108, 208, 232, 32, 236, 205, 167, 174, 161, 116, 13, 249, 187, 22, 240, 185, 172, 160, 103, 94, 162, 147, 26, 15, 143, 183, 147, 98, 231, 117, 134, 185, 50, 64, 40, 30, 27, 13, 152, 132, 40, 138, 32, 78, 158, 162, 207, 212, 229, 210, 251, 88, 116, 67, 229, 164, 164, 147, 59, 32, 94, 217, 197, 242, 149, 102, 74, 219, 46, 127, 68, 28, 116, 10, 2, 249, 231, 130, 123, 29, 45, 73, 56, 17, 195, 208, 45, 25, 60, 252, 98, 189, 109, 25, 0, 253, 151, 254, 124, 211, 48, 23, 156, 78, 163, 154, 188, 17, 69, 14, 188, 16, 64, 59, 190, 136, 70, 162, 253, 237, 156, 111, 41, 27, 40, 63, 205, 204, 94, 0, 50, 237, 62, 87, 211, 115, 91, 68, 194, 104, 119, 72, 106, 226, 160, 48, 165, 138, 134, 2, 138, 153, 181, 38, 249, 48, 120, 72, 15, 245, 227, 15, 164, 64, 188, 74, 4, 84, 213, 83, 67, 73, 87, 181, 72, 94, 46, 54, 193, 252, 188, 14, 207, 28, 82, 159, 131, 168, 238, 168, 145, 28, 230, 27, 126, 151, 93, 5, 96, 68, 126, 66, 174, 155, 101, 123, 20, 218, 131, 92, 124, 78, 82, 44, 55, 139, 77, 105, 177, 136, 121, 177, 43, 77, 12, 240, 0, 76, 20, 133, 121, 129, 73, 15, 160, 200, 150, 114, 95, 59, 59, 165, 240, 204, 13, 156, 134, 194, 4, 70, 158, 213, 111, 229, 103, 216, 239, 132, 16, 184, 151, 206, 254, 229, 62, 23, 58, 125, 49, 144, 208, 215, 2, 3, 1, 0, 1, 2, 130, 2, 1, 0, 185, 115, 209, 92, 24, 92, 19, 159, 131, 89, 166, 193, 68, 164, 46, 135, 24, 20, 243, 42, 94, 230, 4, 200, 73, 103, 159, 121, 131, 251, 83, 222, 153, 30, 171, 191, 176, 16, 114, 103, 152, 161, 118, 12, 148, 246, 152, 0, 100, 101, 113, 224, 74, 125, 174, 117, 74, 156, 125, 165, 54, 189, 179, 172, 255, 80, 135, 42, 178, 247, 217, 204, 209, 163, 49, 155, 42, 72, 88, 176, 46, 63, 255, 195, 192, 184, 248, 183, 223, 76, 226, 197, 54, 245, 206, 60, 8, 10, 181, 122, 1, 223, 113, 196, 133, 143, 58, 77, 185, 235, 78, 76, 32, 59, 212, 66, 110, 162, 75, 123, 210, 153, 180, 58, 97, 179, 129, 60, 175, 142, 228, 123, 85, 50, 241, 119, 147, 204, 94, 43, 65, 163, 4, 167, 243, 247, 41, 134, 105, 197, 165, 63, 45, 145, 56, 174, 203, 192, 135, 209, 29, 195, 83, 179, 14, 184, 131, 104, 152, 48, 245, 179, 207, 178, 60, 23, 21, 1, 84, 207, 82, 124, 9, 137, 171, 141, 187, 55, 172, 180, 180, 10, 48, 185, 97, 79, 156, 39, 249, 192, 27, 98, 77, 250, 93, 18, 157, 130, 72, 210, 115, 96, 36, 132, 116, 101, 225, 96, 234, 79, 89, 243, 89, 135, 97, 252, 53, 72, 97, 34, 226, 41, 41, 45, 144, 243, 189, 162, 243, 43, 69, 136, 143, 182, 140, 223, 134, 93, 38, 245, 36, 125, 46, 93, 48, 94, 215, 39, 156, 57, 86, 93, 207, 204, 72, 106, 112, 215, 203, 230, 80, 20, 137, 224, 242, 33, 146, 33, 108, 188, 185, 254, 117, 189, 240, 82, 64, 60, 186, 247, 190, 138, 170, 159, 147, 75, 49, 148, 101, 174, 130, 21, 177, 211, 121, 6, 153, 144, 230, 166, 181, 155, 94, 232, 2, 4, 119, 236, 35, 133, 253, 223, 14, 30, 199, 57, 215, 31, 251, 90, 167, 19, 231, 154, 54, 225, 85, 68, 17, 234, 158, 53, 50, 243, 182, 149, 193, 214, 60, 188, 6, 38, 2, 200, 161, 232, 193, 30, 153, 231, 221, 57, 140, 55, 69, 35, 21, 153, 34, 238, 175, 65, 253, 210, 119, 125, 120, 116, 153, 127, 67, 204, 9, 66, 210, 200, 165, 212, 216, 2, 62, 19, 15, 171, 77, 183, 247, 127, 224, 138, 41, 208, 170, 227, 36, 158, 176, 111, 128, 172, 70, 73, 241, 148, 172, 50, 174, 126, 80, 177, 235, 93, 89, 102, 84, 76, 221, 30, 216, 49, 125, 142, 35, 45, 96, 224, 60, 161, 63, 48, 85, 143, 20, 76, 182, 111, 15, 156, 139, 55, 155, 113, 226, 248, 239, 130, 252, 241, 197, 247, 124, 61, 39, 197, 170, 119, 76, 136, 195, 180, 169, 106, 240, 234, 101, 114, 207, 11, 160, 170, 139, 194, 187, 48, 22, 114, 84, 64, 151, 30, 212, 99, 213, 176, 106, 79, 232, 127, 197, 153, 133, 8, 56, 210, 83, 67, 106, 124, 231, 96, 2, 145, 2, 130, 1, 1, 0, 244, 218, 215, 194, 174, 36, 99, 217, 1, 4, 236, 11, 160, 86, 85, 65, 206, 36, 36, 143, 205, 108, 166, 191, 91, 209, 75, 117, 7, 81, 33, 179, 44, 101, 145, 215, 39, 117, 195, 81, 31, 111, 36, 7, 26, 105, 30, 249, 91, 2, 2, 237, 126, 141, 231, 153, 213, 181, 100, 234, 219, 192, 114, 179, 215, 229, 39, 212, 107, 9, 55, 220, 136, 233, 237, 28, 74, 97, 6, 22, 26, 47, 150, 83, 82, 95, 186, 146, 22, 38, 176, 231, 255, 166, 199, 223, 217, 86, 142, 56, 43, 199, 25, 247, 249, 122, 59, 142, 152, 20, 49, 147, 13, 132, 249, 203, 251, 146, 116, 96, 88, 81, 232, 45, 106, 100, 187, 99, 73, 32, 203, 134, 30, 223, 100, 179, 179, 128, 81, 242, 25, 85, 137, 125, 96, 153, 240, 224, 86, 20, 206, 24, 26, 197, 233, 164, 158, 50, 222, 103, 197, 211, 144, 101, 182, 205, 201, 51, 23, 231, 125, 229, 130, 61, 139, 204, 195, 243, 69, 38, 185, 187, 48, 249, 140, 107, 137, 39, 234, 21, 13, 43, 24, 112, 108, 109, 15, 25, 57, 55, 127, 40, 152, 238, 227, 96, 86, 157, 114, 35, 52, 54, 38, 140, 85, 42, 119, 53, 99, 35, 133, 208, 240, 65, 171, 8, 71, 255, 243, 248, 176, 166, 17, 178, 92, 62, 203, 56, 158, 31, 169, 223, 123, 7, 118, 216, 166, 132, 83, 62, 112, 160, 99, 244, 132, 29, 2, 130, 1, 1, 0, 228, 158, 249, 243, 243, 94, 42, 189, 87, 61, 152, 139, 197, 122, 33, 97, 4, 39, 135, 66, 219, 225, 11, 70, 103, 92, 115, 10, 8, 225, 5, 2, 220, 32, 23, 147, 56, 111, 237, 98, 48, 174, 122, 207, 109, 152, 187, 125, 220, 186, 73, 127, 42, 82, 39, 228, 163, 12, 188, 36, 71, 107, 52, 235, 223, 200, 7, 38, 6, 167, 28, 158, 26, 213, 126, 186, 90, 152, 133, 44, 53, 156, 61, 130, 92, 163, 3, 27, 35, 185, 141, 112, 236, 246, 210, 107, 75, 245, 33, 126, 134, 215, 41, 1, 244, 220, 36, 93, 22, 232, 50, 62, 68, 141, 153, 118, 62, 1, 167, 197, 202, 113, 187, 196, 186, 251, 161, 128, 66, 211, 145, 103, 133, 69, 207, 155, 117, 65, 76, 251, 125, 43, 224, 105, 171, 6, 29, 254, 31, 111, 144, 5, 158, 166, 180, 143, 163, 205, 212, 151, 7, 11, 50, 234, 82, 37, 143, 75, 104, 124, 97, 69, 220, 246, 202, 45, 25, 40, 220, 23, 92, 116, 112, 114, 204, 198, 140, 48, 111, 191, 53, 28, 9, 134, 234, 90, 168, 243, 108, 75, 197, 99, 162, 173, 31, 194, 97, 224, 184, 76, 227, 170, 199, 106, 129, 14, 77, 234, 231, 38, 192, 197, 233, 174, 150, 240, 55, 252, 241, 27, 97, 169, 49, 49, 115, 9, 218, 65, 253, 14, 253, 217, 91, 141, 44, 68, 32, 247, 219, 199, 31, 45, 212, 68, 46, 131, 2, 130, 1, 1, 0, 225, 142, 199, 187, 155, 88, 2, 114, 225, 49, 123, 144, 170, 63, 93, 130, 165, 55, 62, 71, 10, 97, 208, 169, 239, 23, 58, 127, 176, 33, 216, 253, 137, 36, 119, 216, 207, 140, 248, 68, 62, 196, 207, 87, 139, 200, 210, 179, 186, 86, 124, 3, 243, 213, 29, 72, 229, 73, 152, 145, 145, 166, 19, 4, 1, 26, 36, 58, 213, 239, 67, 250, 112, 85, 174, 11, 165, 169, 3, 70, 81, 17, 13, 85, 236, 72, 43, 66, 112, 13, 108, 98, 11, 107, 196, 44, 61, 182, 50, 133, 36, 46, 225, 137, 65, 212, 140, 16, 171, 159, 206, 155, 60, 149, 6, 216, 22, 3, 176, 25, 32, 195, 51, 50, 195, 19, 208, 91, 129, 254, 39, 254, 129, 106, 33, 6, 57, 145, 55, 235, 225, 210, 158, 57, 85, 71, 250, 81, 110, 122, 243, 239, 216, 154, 0, 197, 152, 198, 27, 131, 85, 5, 179, 187, 63, 79, 10, 205, 122, 115, 209, 210, 30, 204, 59, 128, 129, 242, 19, 253, 188, 146, 232, 102, 186, 40, 69, 204, 243, 34, 57, 99, 61, 188, 50, 229, 180, 70, 244, 34, 95, 141, 50, 116, 190, 24, 253, 49, 68, 247, 145, 29, 97, 29, 93, 71, 37, 81, 148, 230, 32, 91, 125, 55, 193, 42, 123, 201, 25, 34, 58, 248, 128, 204, 225, 149, 38, 248, 29, 17, 230, 22, 236, 234, 207, 92, 124, 232, 225, 22, 96, 2, 32, 146, 27, 49, 2, 130, 1, 1, 0, 129, 62, 34, 61, 183, 242, 31, 37, 68, 193, 108, 144, 111, 133, 248, 130, 184, 239, 131, 182, 215, 72, 164, 176, 27, 84, 151, 48, 48, 14, 205, 95, 109, 131, 178, 240, 38, 50, 152, 55, 47, 32, 36, 11, 73, 128, 211, 85, 118, 199, 213, 46, 207, 132, 252, 74, 115, 166, 138, 97, 212, 2, 22, 59, 214, 25, 101, 121, 40, 191, 166, 28, 247, 60, 132, 84, 227, 76, 95, 212, 187, 69, 229, 59, 226, 20, 193, 119, 193, 61, 111, 105, 76, 124, 200, 61, 162, 6, 36, 246, 59, 82, 61, 59, 126, 234, 72, 160, 91, 135, 206, 135, 135, 7, 169, 158, 191, 180, 253, 220, 129, 242, 195, 220, 150, 124, 20, 51, 199, 19, 133, 154, 201, 43, 203, 14, 174, 61, 201, 64, 78, 229, 212, 10, 200, 133, 63, 197, 94, 142, 26, 20, 35, 57, 72, 207, 255, 33, 40, 50, 108, 231, 246, 211, 162, 182, 219, 8, 29, 60, 91, 93, 60, 106, 67, 167, 53, 22, 245, 61, 59, 166, 19, 191, 194, 101, 231, 240, 165, 235, 169, 33, 125, 125, 72, 213, 17, 183, 243, 27, 238, 173, 193, 212, 47, 37, 27, 98, 7, 174, 103, 242, 46, 163, 213, 235, 121, 62, 247, 135, 223, 232, 194, 143, 81, 130, 225, 147, 219, 213, 199, 226, 247, 13, 102, 100, 70, 127, 145, 136, 189, 22, 248, 123, 153, 111, 182, 87, 136, 102, 76, 9, 3, 123, 187, 243, 2, 130, 1, 0, 36, 121, 149, 41, 189, 115, 193, 110, 98, 69, 30, 145, 9, 231, 177, 98, 120, 118, 126, 102, 62, 220, 58, 207, 73, 211, 60, 15, 24, 107, 208, 95, 29, 107, 40, 190, 182, 84, 106, 17, 217, 198, 210, 27, 233, 227, 153, 252, 128, 181, 44, 145, 101, 156, 7, 209, 23, 149, 66, 78, 109, 145, 138, 13, 241, 174, 198, 3, 26, 222, 15, 241, 120, 176, 54, 190, 97, 80, 215, 99, 49, 62, 204, 135, 226, 32, 141, 102, 251, 32, 152, 108, 113, 237, 59, 142, 30, 185, 195, 135, 145, 1, 86, 115, 56, 253, 215, 186, 221, 202, 196, 36, 227, 118, 177, 130, 60, 59, 56, 190, 198, 157, 142, 18, 96, 43, 218, 199, 150, 42, 174, 44, 198, 65, 103, 139, 167, 177, 46, 26, 155, 248, 209, 56, 155, 209, 204, 42, 89, 224, 212, 75, 80, 135, 106, 203, 4, 81, 181, 85, 128, 247, 73, 134, 41, 48, 183, 57, 127, 28, 234, 26, 244, 177, 159, 113, 90, 249, 120, 32, 248, 134, 79, 99, 123, 155, 173, 201, 185, 216, 166, 32, 152, 181, 6, 154, 118, 18, 181, 245, 106, 25, 37, 146, 118, 16, 215, 30, 83, 96, 35, 154, 93, 0, 13, 5, 206, 156, 129, 147, 118, 87, 248, 155, 49, 135, 7, 39, 157, 226, 171, 96, 16, 112, 122, 173, 58, 145, 19, 6, 90, 11, 221, 109, 208, 16, 251, 188, 18, 120, 106, 170, 143, 149, 79, 192]),
            jwk: {
                kty: "RSA",
                n: "2qr2TL2c2JmbsN0OLIRnaAB_ZKb1-Gh9H0qb4lrBuDaqkW_eFPwT-JIsvnNJvDT7BLJ57tTMIj56ZMtv6efSSTWSk9MOoW2J1K_iEretZ2cegB_aRX7qQVjnoFsz9U02BKfAIUT0o_K7b9G08d1rrAUohi_SVQhwObodg7BddMbKUmz70QNIS487LN44WUVnn9OgE9atTYUARNukT0DuQb3J-K20ksTuVujXbSelohDmLobqlGoi5sY_548Qs9BtFmQ2nGuEHNB2zdlZ5EvEqbUFVZ2QboG6jXdoos6qcwdgUvAhj1Hz10Ngic_RFqL7bNDoIOzNp66hdA35uxbwuaygZ16ikxoPj7eTYud1hrkyQCgeGw2YhCiKIE6eos_U5dL7WHRD5aSkkzsgXtnF8pVmStsuf0QcdAoC-eeCex0tSTgRw9AtGTz8Yr1tGQD9l_580zAXnE6jmrwRRQ68EEA7vohGov3tnG8pGyg_zcxeADLtPlfTc1tEwmh3SGrioDClioYCipm1JvkweEgP9eMPpEC8SgRU1VNDSVe1SF4uNsH8vA7PHFKfg6juqJEc5ht-l10FYER-Qq6bZXsU2oNcfE5SLDeLTWmxiHmxK00M8ABMFIV5gUkPoMiWcl87O6XwzA2chsIERp7Vb-Vn2O-EELiXzv7lPhc6fTGQ0Nc",
                e: "AQAB",
                d: "uXPRXBhcE5-DWabBRKQuhxgU8ype5gTISWefeYP7U96ZHqu_sBByZ5ihdgyU9pgAZGVx4Ep9rnVKnH2lNr2zrP9Qhyqy99nM0aMxmypIWLAuP__DwLj4t99M4sU29c48CAq1egHfccSFjzpNuetOTCA71EJuokt70pm0OmGzgTyvjuR7VTLxd5PMXitBowSn8_cphmnFpT8tkTiuy8CH0R3DU7MOuINomDD1s8-yPBcVAVTPUnwJiauNuzestLQKMLlhT5wn-cAbYk36XRKdgkjSc2AkhHRl4WDqT1nzWYdh_DVIYSLiKSktkPO9ovMrRYiPtozfhl0m9SR9Ll0wXtcnnDlWXc_MSGpw18vmUBSJ4PIhkiFsvLn-db3wUkA8uve-iqqfk0sxlGWughWx03kGmZDmprWbXugCBHfsI4X93w4exznXH_tapxPnmjbhVUQR6p41MvO2lcHWPLwGJgLIoejBHpnn3TmMN0UjFZki7q9B_dJ3fXh0mX9DzAlC0sil1NgCPhMPq02393_giinQquMknrBvgKxGSfGUrDKuflCx611ZZlRM3R7YMX2OIy1g4DyhPzBVjxRMtm8PnIs3m3Hi-O-C_PHF93w9J8Wqd0yIw7SpavDqZXLPC6Cqi8K7MBZyVECXHtRj1bBqT-h_xZmFCDjSU0NqfOdgApE",
                p: "9NrXwq4kY9kBBOwLoFZVQc4kJI_NbKa_W9FLdQdRIbMsZZHXJ3XDUR9vJAcaaR75WwIC7X6N55nVtWTq28Bys9flJ9RrCTfciOntHEphBhYaL5ZTUl-6khYmsOf_psff2VaOOCvHGff5ejuOmBQxkw2E-cv7knRgWFHoLWpku2NJIMuGHt9ks7OAUfIZVYl9YJnw4FYUzhgaxemknjLeZ8XTkGW2zckzF-d95YI9i8zD80Umubsw-YxriSfqFQ0rGHBsbQ8ZOTd_KJju42BWnXIjNDYmjFUqdzVjI4XQ8EGrCEf_8_iwphGyXD7LOJ4fqd97B3bYpoRTPnCgY_SEHQ",
                q: "5J758_NeKr1XPZiLxXohYQQnh0Lb4QtGZ1xzCgjhBQLcIBeTOG_tYjCues9tmLt93LpJfypSJ-SjDLwkR2s069_IByYGpxyeGtV-ulqYhSw1nD2CXKMDGyO5jXDs9tJrS_UhfobXKQH03CRdFugyPkSNmXY-AafFynG7xLr7oYBC05FnhUXPm3VBTPt9K-BpqwYd_h9vkAWeprSPo83UlwcLMupSJY9LaHxhRdz2yi0ZKNwXXHRwcszGjDBvvzUcCYbqWqjzbEvFY6KtH8Jh4LhM46rHaoEOTernJsDF6a6W8Df88RthqTExcwnaQf0O_dlbjSxEIPfbxx8t1EQugw",
                dp: "4Y7Hu5tYAnLhMXuQqj9dgqU3PkcKYdCp7xc6f7Ah2P2JJHfYz4z4RD7Ez1eLyNKzulZ8A_PVHUjlSZiRkaYTBAEaJDrV70P6cFWuC6WpA0ZREQ1V7EgrQnANbGILa8QsPbYyhSQu4YlB1IwQq5_OmzyVBtgWA7AZIMMzMsMT0FuB_if-gWohBjmRN-vh0p45VUf6UW568-_YmgDFmMYbg1UFs7s_TwrNenPR0h7MO4CB8hP9vJLoZrooRczzIjljPbwy5bRG9CJfjTJ0vhj9MUT3kR1hHV1HJVGU5iBbfTfBKnvJGSI6-IDM4ZUm-B0R5hbs6s9cfOjhFmACIJIbMQ",
                dq: "gT4iPbfyHyVEwWyQb4X4grjvg7bXSKSwG1SXMDAOzV9tg7LwJjKYNy8gJAtJgNNVdsfVLs-E_Epzpoph1AIWO9YZZXkov6Yc9zyEVONMX9S7ReU74hTBd8E9b2lMfMg9ogYk9jtSPTt-6kigW4fOh4cHqZ6_tP3cgfLD3JZ8FDPHE4WaySvLDq49yUBO5dQKyIU_xV6OGhQjOUjP_yEoMmzn9tOittsIHTxbXTxqQ6c1FvU9O6YTv8Jl5_Cl66khfX1I1RG38xvurcHULyUbYgeuZ_Iuo9XreT73h9_owo9RguGT29XH4vcNZmRGf5GIvRb4e5lvtleIZkwJA3u78w",
                qi: "JHmVKb1zwW5iRR6RCeexYnh2fmY-3DrPSdM8Dxhr0F8dayi-tlRqEdnG0hvp45n8gLUskWWcB9EXlUJObZGKDfGuxgMa3g_xeLA2vmFQ12MxPsyH4iCNZvsgmGxx7TuOHrnDh5EBVnM4_de63crEJON2sYI8Ozi-xp2OEmAr2seWKq4sxkFni6exLhqb-NE4m9HMKlng1EtQh2rLBFG1VYD3SYYpMLc5fxzqGvSxn3Fa-Xgg-IZPY3ubrcm52KYgmLUGmnYStfVqGSWSdhDXHlNgI5pdAA0FzpyBk3ZX-JsxhwcnneKrYBBweq06kRMGWgvdbdAQ-7wSeGqqj5VPwA"
            }
        },

    };

    // combinations to test
    var testVectors = [
        {name: "RSA-OAEP", privateUsages: ["decrypt", "unwrapKey"], publicUsages: ["encrypt", "wrapKey"]},
        {name: "RSA-PSS",  privateUsages: ["sign"], publicUsages: ["verify"]},
        {name: "RSASSA-PKCS1-v1_5",  privateUsages: ["sign"], publicUsages: ["verify"]}
    ];

    // TESTS ARE HERE:
    // Test every test vector, along with all available key data
    testVectors.forEach(function(vector) {
        sizes.forEach(function(size) {

            hashes.forEach(function(hash) {
                [true, false].forEach(function(extractable) {

                    // Test public keys first
                    allValidUsages(vector.publicUsages, true).forEach(function(usages) {
                        ['spki', 'jwk'].forEach(function(format) {
                            var algorithm = {name: vector.name, hash: hash};
                            var data = keyData[size];
                            if (format === "jwk") { // Not all fields used for public keys
                                data = {jwk: {kty: keyData[size].jwk.kty, n: keyData[size].jwk.n, e: keyData[size].jwk.e}};
                            }

                            testFormat(format, algorithm, data, size, usages, extractable);
                        });

                    });

                    // Next, test private keys
                    ['pkcs8', 'jwk'].forEach(function(format) {
                        var algorithm = {name: vector.name, hash: hash};
                        var data = keyData[size];
                        allValidUsages(vector.privateUsages).forEach(function(usages) {
                            testFormat(format, algorithm, data, size, usages, extractable);
                        });
                        testEmptyUsages(format, algorithm, data, size, extractable);
                    });
                });
            });

        });
    });


    // Test importKey with a given key format and other parameters. If
    // extrable is true, export the key and verify that it matches the input.
    function testFormat(format, algorithm, keyData, keySize, usages, extractable) {
        promise_test(function(test) {
            return subtle.importKey(format, keyData[format], algorithm, extractable, usages).
            then(function(key) {
                assert_equals(key.constructor, CryptoKey, "Imported a CryptoKey object");
                assert_goodCryptoKey(key, algorithm, extractable, usages, (format === 'pkcs8' || (format === 'jwk' && keyData[format].d)) ? 'private' : 'public');
                if (!extractable) {
                    return;
                }

                return subtle.exportKey(format, key).
                then(function(result) {
                    if (format !== "jwk") {
                        assert_true(equalBuffers(keyData[format], result), "Round trip works");
                    } else {
                        assert_true(equalJwk(keyData[format], result), "Round trip works");
                    }
                }, function(err) {
                    assert_unreached("Threw an unexpected error: " + err.toString());
                });
            }, function(err) {
                assert_unreached("Threw an unexpected error: " + err.toString());
            });
        }, "Good parameters: " + keySize.toString() + " bits " + parameterString(format, keyData[format], algorithm, extractable, usages));
    }

    // Test importKey with a given key format and other parameters but with empty usages.
    // Should fail with SyntaxError
    function testEmptyUsages(format, algorithm, keyData, keySize, extractable) {
        const usages = [];
        promise_test(function(test) {
            return subtle.importKey(format, keyData[format], algorithm, extractable, usages).
            then(function(key) {
                assert_unreached("importKey succeeded but should have failed with SyntaxError");
            }, function(err) {
                assert_equals(err.name, "SyntaxError", "Should throw correct error, not " + err.name + ": " + err.message);
            });
        }, "Empty Usages: " + keySize.toString() + " bits " + parameterString(format, keyData, algorithm, extractable, usages));
    }



    // Helper methods follow:

    // Are two array buffers the same?
    function equalBuffers(a, b) {
        if (a.byteLength !== b.byteLength) {
            return false;
        }

        var aBytes = new Uint8Array(a);
        var bBytes = new Uint8Array(b);

        for (var i=0; i<a.byteLength; i++) {
            if (aBytes[i] !== bBytes[i]) {
                return false;
            }
        }

        return true;
    }

    // Are two Jwk objects "the same"? That is, does the object returned include
    // matching values for each property that was expected? It's okay if the
    // returned object has extra methods; they aren't checked.
    function equalJwk(expected, got) {
        var fields = Object.keys(expected);
        var fieldName;

        for(var i=0; i<fields.length; i++) {
            fieldName = fields[i];
            if (!(fieldName in got)) {
                return false;
            }
            if (expected[fieldName] !== got[fieldName]) {
                return false;
            }
        }

        return true;
    }

    // Build minimal Jwk objects from raw key data and algorithm specifications
    function jwkData(keyData, algorithm) {
        var result = {
            kty: "oct",
            k: byteArrayToUnpaddedBase64(keyData)
        };

        if (algorithm.name.substring(0, 3) === "AES") {
            result.alg = "A" + (8 * keyData.byteLength).toString() + algorithm.name.substring(4);
        } else if (algorithm.name === "HMAC") {
            result.alg = "HS" + algorithm.hash.substring(4);
        }
        return result;
    }

    // Jwk format wants Base 64 without the typical padding at the end.
    function byteArrayToUnpaddedBase64(byteArray){
        var binaryString = "";
        for (var i=0; i<byteArray.byteLength; i++){
            binaryString += String.fromCharCode(byteArray[i]);
        }
        var base64String = btoa(binaryString);

        return base64String.replace(/=/g, "");
    }

    // Convert method parameters to a string to uniquely name each test
    function parameterString(format, data, algorithm, extractable, usages) {
        if ("byteLength" in data) {
            data = "buffer(" + data.byteLength.toString() + ")";
        } else {
            data = "object(" + Object.keys(data).join(", ") + ")";
        }
        var result = "(" +
                        objectToString(format) + ", " +
                        objectToString(data) + ", " +
                        objectToString(algorithm) + ", " +
                        objectToString(extractable) + ", " +
                        objectToString(usages) +
                     ")";

        return result;
    }

    // Character representation of any object we may use as a parameter.
    function objectToString(obj) {
        var keyValuePairs = [];

        if (Array.isArray(obj)) {
            return "[" + obj.map(function(elem){return objectToString(elem);}).join(", ") + "]";
        } else if (typeof obj === "object") {
            Object.keys(obj).sort().forEach(function(keyName) {
                keyValuePairs.push(keyName + ": " + objectToString(obj[keyName]));
            });
            return "{" + keyValuePairs.join(", ") + "}";
        } else if (typeof obj === "undefined") {
            return "undefined";
        } else {
            return obj.toString();
        }

        var keyValuePairs = [];

        Object.keys(obj).sort().forEach(function(keyName) {
            var value = obj[keyName];
            if (typeof value === "object") {
                value = objectToString(value);
            } else if (typeof value === "array") {
                value = "[" + value.map(function(elem){return objectToString(elem);}).join(", ") + "]";
            } else {
                value = value.toString();
            }

            keyValuePairs.push(keyName + ": " + value);
        });

        return "{" + keyValuePairs.join(", ") + "}";
    }
