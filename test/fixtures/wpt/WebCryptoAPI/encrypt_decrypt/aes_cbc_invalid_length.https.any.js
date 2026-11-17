// META: title=WebCryptoAPI: AES-CBC decryption ciphertext lengths
// META: global=window,dedicatedworker

/*
 * Copyright (C) 2026 Jonathan Powers. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// https://w3c.github.io/webcrypto/#aes-cbc-operations
// Decryption requires a nonempty ciphertext whose length is a multiple of 16 bytes.
for (const keyLength of [128, 192, 256]) {
    for (const cipherTextLength of [0, 1, 15, 17, 31, 33]) {
        promise_test(async t => {
            const key = await crypto.subtle.importKey("raw", new Uint8Array(keyLength / 8), "AES-CBC", false, ["decrypt"]);
            const parameters = { name: "AES-CBC", iv: new Uint8Array(16) };
            await promise_rejects_dom(t, "OperationError", crypto.subtle.decrypt(parameters, key, new Uint8Array(cipherTextLength)));
        }, `AES-CBC with a ${keyLength}-bit key rejects ${cipherTextLength}-byte ciphertext with OperationError`);
    }

    promise_test(async () => {
        const key = await crypto.subtle.importKey("raw", new Uint8Array(keyLength / 8), "AES-CBC", false, ["encrypt", "decrypt"]);
        const parameters = { name: "AES-CBC", iv: new Uint8Array(16) };
        const cipherText = await crypto.subtle.encrypt(parameters, key, new Uint8Array(0));
        assert_equals(cipherText.byteLength, 16, "Encrypting empty plaintext produces a full padding block");
        const plainText = await crypto.subtle.decrypt(parameters, key, cipherText);
        assert_equals(plainText.byteLength, 0, "Decrypting a full padding block produces empty plaintext");
    }, `AES-CBC with a ${keyLength}-bit key decrypts a padded empty message`);
}
