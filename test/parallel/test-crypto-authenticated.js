// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

//
// Test authenticated encryption modes.
//
// !NEVER USE STATIC IVs IN REAL LIFE!
//

const TEST_CASES = [
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13896f64dfa2c2d0f2c76',
    tag: '272b422f62eb545eaa15b5ff84092447', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13896f64dfa2c2d0f2c76', aad: '000000FF',
    tag: 'ba2479f66275665a88cb7b15f43eb005', tampered: false },
  { algo: 'aes-128-gcm',
    key: '6970787039613669314d623455536234',
    iv: '583673497131313748307652', plain: 'Hello World!',
    ct: '4be13596f64dfa2c2d0fac76',
    tag: '272b422f62eb545eaa15b5ff84092447', tampered: true },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58e62cfe7b1d274111a82267ebb93866e72b6c2a',
    tag: '9bb44f663badabacae9720881fb1ec7a', tampered: false },
  { algo: 'aes-256-gcm',
    key: '337a54767a7233703637564336316a6d56353472495975313534357834546c59',
    iv: '36306950306836764a6f4561', plain: 'Hello node.js world!',
    ct: '58e62cff7b1d274011a82267ebb93866e72b6c2b',
    tag: '9bb44f663badabacae9720881fb1ec7a', tampered: true },
  { algo: 'aes-192-gcm',
    key: '1ed2233fa2223ef5d7df08546049406c7305220bca40d4c9',
    iv: '0e1791e9db3bd21a9122c416', plain: 'Hello node.js world!',
    password: 'very bad password', aad: '63616c76696e',
    ct: 'dda53a4059aa17b88756984995f7bba3c636cc44',
    tag: 'd2a35e5c611e5e3d2258360241c5b045', tampered: false },

  // Following test cases are from "The Galois/Counter Mode of Operation (GCM)"
  // by D. McGrew and J. Viega, published by NIST.

  // Test case 1
  { algo: 'aes-128-gcm',
    key: '00000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: '58e2fccefa7e3061367f1d57a4e7455a', tampered: false },

  // Test case 2
  { algo: 'aes-128-gcm',
    key: '00000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: '0388dace60b6a392f328c2b971b2fe78',
    tag: 'ab6e47d42cec13bdf53a67b21257bddf', tampered: false },

  // Test case 3
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a721' +
           'c3c0c95956809532fcf0e2449a6b525b1' +
           '6aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '42831ec2217774244b7221b784d0d49c' +
        'e3aa212f2c02a4e035c17e2329aca12e2' +
        '1d514b25466931c7d8f6a5aac84aa051b' +
        'a30b396a0aac973d58e091473f5985',
    tag: '4d5c2af327cd64a62cf35abd2ba6fab4', tampered: false },

  // Test case 4
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a721' +
           'c3c0c95956809532fcf0e2449a6b525b16' +
           'aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '42831ec2217774244b7221b784d0d49c' +
        'e3aa212f2c02a4e035c17e2329aca12e2' +
        '1d514b25466931c7d8f6a5aac84aa051b' +
        'a30b396a0aac973d58e091',
    tag: '5bc94fbc3221a5db94fae95ae7121a47', tampered: false },

  // Test case 5, 8 byte IV
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeef' +
         'abaddad2',
    plainIsHex: true,
    ct: '61353b4c2806934a777ff51fa22a4755' +
        '699b2a714fcdc6f83766e5f97b6c7423' +
        '73806900e49f24b22b097544d4896b42' +
        '4989b5e1ebac0f07c23f4598',
    tag: '3612d2e79e3b0785561be14aaca2fccb', tampered: false },

  // Test case 6, 60 byte IV
  { algo: 'aes-128-gcm',
    key: 'feffe9928665731c6d6a8f9467308308',
    iv: '9313225DF88406E555909C5AFF5269AA' +
        '6A7A9538534F7DA1E4C303D2A318A728' +
        'C3C0C95156809539FCF0E2429A6B52541' +
        '6AEDBF5A0DE6A57A637B39B',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '8ce24998625615b603a033aca13fb894' +
        'be9112a5c3a211a8ba262a3cca7e2ca7' +
        '01e4a9a4fba43c90ccdcb281d48c7c6f' +
        'd62875d2aca417034c34aee5',
    tag: '619cc5aefffe0bfa462af43c1699d050', tampered: false },

  // Test case 7
  { algo: 'aes-192-gcm',
    key: '000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: 'cd33b28ac773f74ba00ed1f312572435', tampered: false },

  // Test case 8
  { algo: 'aes-192-gcm',
    key: '000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: '98e7247c07f0fe411c267e4384b0f600',
    tag: '2ff58d80033927ab8ef4d4587514f0fb', tampered: false },

  // Test case 9
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '3980ca0b3c00e841eb06fac4872a2757' +
        '859e1ceaa6efd984628593b40ca1e19c' +
        '7d773d00c144c525ac619d18c84a3f47' +
        '18e2448b2fe324d9ccda2710acade256',
    tag: '9924a7c8587336bfb118024db8674a14', tampered: false },

  // Test case 10
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '3980ca0b3c00e841eb06fac4872a2757' +
        '859e1ceaa6efd984628593b40ca1e19c' +
        '7d773d00c144c525ac619d18c84a3f47' +
        '18e2448b2fe324d9ccda2710',
    tag: '2519498e80f1478f37ba55bd6d27618c', tampered: false },

  // Test case 11
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '0f10f599ae14a154ed24b36e25324db8' +
        'c566632ef2bbb34f8347280fc4507057' +
        'fddc29df9a471f75c66541d4d4dad1c9' +
        'e93a19a58e8b473fa0f062f7',
    tag: '65dcc57fcf623a24094fcca40d3533f8', tampered: false },

  // Test case 12, 60 byte IV
  { algo: 'aes-192-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c',
    iv: '9313225df88406e555909c5aff5269aa' +
        '6a7a9538534f7da1e4c303d2a318a728' +
        'c3c0c95156809539fcf0e2429a6b5254' +
        '16aedbf5a0de6a57a637b39b',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: 'd27e88681ce3243c4830165a8fdcf9ff' +
        '1de9a1d8e6b447ef6ef7b79828666e45' +
        '81e79012af34ddd9e2f037589b292db3' +
        'e67c036745fa22e7e9b7373b',
    tag: 'dcf566ff291c25bbb8568fc3d376a6d9', tampered: false },

  // Test case 13
  { algo: 'aes-256-gcm',
    key: '0000000000000000000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '',
    plainIsHex: false,
    ct: '',
    tag: '530f8afbc74536b9a963b4f1c4cb738b', tampered: false },

  // Test case 14
  { algo: 'aes-256-gcm',
    key: '0000000000000000000000000000000000000000000000000000000000000000',
    iv: '000000000000000000000000',
    plain: '00000000000000000000000000000000',
    plainIsHex: true,
    ct: 'cea7403d4d606b6e074ec5d3baf39d18',
    tag: 'd0d1c8a799996bf0265b98b5d48ab919', tampered: false },

  // Test case 15
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b391aafd255',
    plainIsHex: true,
    ct: '522dc1f099567d07f47f37a32a84427d' +
        '643a8cdcbfe5c0c97598a2bd2555d1aa' +
        '8cb08e48590dbb3da7b08b1056828838' +
        'c5f61e6393ba7a0abcc9f662898015ad',
    tag: 'b094dac5d93471bdec1a502270e3cc6c', tampered: false },

  // Test case 16
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbaddecaf888',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '522dc1f099567d07f47f37a32a84427d' +
        '643a8cdcbfe5c0c97598a2bd2555d1aa' +
        '8cb08e48590dbb3da7b08b1056828838' +
        'c5f61e6393ba7a0abcc9f662',
    tag: '76fc6ece0f4e1768cddf8853bb2d551b', tampered: false },

  // Test case 17, 8 byte IV
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: 'cafebabefacedbad',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: 'c3762df1ca787d32ae47c13bf19844cb' +
        'af1ae14d0b976afac52ff7d79bba9de0' +
        'feb582d33934a4f0954cc2363bc73f78' +
        '62ac430e64abe499f47c9b1f',
    tag: '3a337dbf46a792c45e454913fe2ea8f2', tampered: false },

  // Test case 18, 60 byte IV
  { algo: 'aes-256-gcm',
    key: 'feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308',
    iv: '9313225df88406e555909c5aff5269aa' +
        '6a7a9538534f7da1e4c303d2a318a728' +
        'c3c0c95156809539fcf0e2429a6b5254' +
        '16aedbf5a0de6a57a637b39b',
    plain: 'd9313225f88406e5a55909c5aff5269a' +
           '86a7a9531534f7da2e4c303d8a318a72' +
           '1c3c0c95956809532fcf0e2449a6b525' +
           'b16aedf5aa0de657ba637b39',
    aad: 'feedfacedeadbeeffeedfacedeadbeefabaddad2',
    plainIsHex: true,
    ct: '5a8def2f0c9e53f1f75d7853659e2a20' +
        'eeb2b22aafde6419a058ab4f6f746bf4' +
        '0fc0c3b780f244452da3ebf1c5d82cde' +
        'a2418997200ef82e44ae7e3f',
    tag: 'a44a8266ee1c8eb0c8b5d4cf5ae9f19a', tampered: false },

  // The following test cases for AES-CCM are from RFC3610

  // Packet Vector #1
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000003020100a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '588c979a61c663d2f066d0c2c0f989806d5f6b61dac384',
    tag: '17e8d12cfdf926e0'
  },

  // Packet Vector #2
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000004030201a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '72c91a36e135f8cf291ca894085c87e3cc15c439c9e43a3b',
    tag: 'a091d56e10400916'
  },

  // Packet Vector #3
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000005040302a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '51b1e5f44a197d1da46b0f8e2d282ae871e838bb64da859657',
    tag: '4adaa76fbd9fb0c5'
  },

  // Packet Vector #4
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000006050403a0a1a2a3a4a5',
    plain: '0c0d0e0f101112131415161718191a1b1c1d1e',
    aad: '000102030405060708090a0b',
    plainIsHex: true,
    ct: 'a28c6865939a9a79faaa5c4c2a9d4a91cdac8c',
    tag: '96c861b9c9e61ef1'
  },

  // Packet Vector #5
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000007060504a0a1a2a3a4a5',
    plain: '0c0d0e0f101112131415161718191a1b1c1d1e1f',
    aad: '000102030405060708090a0b',
    plainIsHex: true,
    ct: 'dcf1fb7b5d9e23fb9d4e131253658ad86ebdca3e',
    tag: '51e83f077d9c2d93'
  },

  // Packet Vector #6
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000008070605a0a1a2a3a4a5',
    plain: '0c0d0e0f101112131415161718191a1b1c1d1e1f20',
    aad: '000102030405060708090a0b',
    plainIsHex: true,
    ct: '6fc1b011f006568b5171a42d953d469b2570a4bd87',
    tag: '405a0443ac91cb94'
  },

  // Packet Vector #7
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000009080706a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '0135d1b2c95f41d5d1d4fec185d166b8094e999dfed96c',
    tag: '048c56602c97acbb7490'
  },

  // Packet Vector #7 with invalid authentication tag
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000009080706a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '0135d1b2c95f41d5d1d4fec185d166b8094e999dfed96c',
    tag: '048c56602c97acbb7491',
    tampered: true
  },

  // Packet Vector #7 with invalid ciphertext
  {
    algo: 'aes-128-ccm',
    key: 'c0c1c2c3c4c5c6c7c8c9cacbcccdcecf',
    iv: '00000009080706a0a1a2a3a4a5',
    plain: '08090a0b0c0d0e0f101112131415161718191a1b1c1d1e',
    aad: '0001020304050607',
    plainIsHex: true,
    ct: '0135d1b2c95f41d5d1d4fec185d166b8094e999dfed96d',
    tag: '048c56602c97acbb7490',
    tampered: true
  },

  // Test case for CCM with a password using create(C|Dec)ipher
  {
    algo: 'aes-192-ccm',
    key: '1ed2233fa2223ef5d7df08546049406c7305220bca40d4c9',
    iv: '0e1791e9db3bd21a9122c416',
    plain: 'Hello node.js world!',
    password: 'very bad password',
    aad: '63616c76696e',
    ct: '49d2c2bd4892703af2f25db04cbe00e703d6d5ac',
    tag: '693c21ce212564fc3a6f',
    tampered: false
  },

  // Test case for CCM with a password using create(C|Dec)ipher, invalid tag
  {
    algo: 'aes-192-ccm',
    key: '1ed2233fa2223ef5d7df08546049406c7305220bca40d4c9',
    iv: '0e1791e9db3bd21a9122c416',
    plain: 'Hello node.js world!',
    password: 'very bad password',
    aad: '63616c76696e',
    ct: '49d2c2bd4892703af2f25db04cbe00e703d6d5ac',
    tag: '693c21ce212564fc3a6e',
    tampered: true
  },

  // Same test with a 128-bit key
  {
    algo: 'aes-128-ccm',
    key: '1ed2233fa2223ef5d7df08546049406c',
    iv: '7305220bca40d4c90e1791e9',
    plain: 'Hello node.js world!',
    password: 'very bad password',
    aad: '63616c76696e',
    ct: '8beba09d4d4d861f957d51c0794f4abf8030848e',
    tag: '0d9bcd142a94caf3d1dd',
    tampered: false
  },

  // Test case for CCM without any AAD
  {
    algo: 'aes-128-ccm',
    key: '1ed2233fa2223ef5d7df08546049406c',
    iv: '7305220bca40d4c90e1791e9',
    plain: 'Hello node.js world!',
    password: 'very bad password',
    ct: '8beba09d4d4d861f957d51c0794f4abf8030848e',
    tag: '29d71a70bb58dae1425d',
    tampered: false
  },

  // Test case for CCM with an empty message
  {
    algo: 'aes-128-ccm',
    key: '1ed2233fa2223ef5d7df08546049406c',
    iv: '7305220bca40d4c90e1791e9',
    plain: '',
    password: 'very bad password',
    aad: '63616c76696e',
    ct: '',
    tag: '65a6002b2cdfe9f00027f839332ca6fc',
    tampered: false
  },
];

const errMessages = {
  auth: / auth/,
  state: / state/,
  FIPS: /not supported in FIPS mode/,
  length: /Invalid IV length/,
  authTagLength: /Invalid authentication tag length/
};

const ciphers = crypto.getCiphers();

const expectedWarnings = common.hasFipsCrypto ?
  [] : [
    ['Use Cipheriv for counter mode of aes-192-gcm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-192-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-192-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-128-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-128-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-128-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode],
    ['Use Cipheriv for counter mode of aes-256-ccm', common.noWarnCode]
  ];

const expectedDeprecationWarnings = [0, 1, 2, 6, 9, 10, 11, 17]
  .map((i) => [`Permitting authentication tag lengths of ${i} bytes is ` +
            'deprecated. Valid GCM tag lengths are 4, 8, 12, 13, 14, 15, 16.',
               'DEP0090']);

expectedDeprecationWarnings.push(['crypto.DEFAULT_ENCODING is deprecated.',
                                  'DEP0091']);

common.expectWarning({
  Warning: expectedWarnings,
  DeprecationWarning: expectedDeprecationWarnings
});

for (const test of TEST_CASES) {
  if (!ciphers.includes(test.algo)) {
    common.printSkipMessage(`unsupported ${test.algo} test`);
    continue;
  }

  if (common.hasFipsCrypto && test.iv.length < 24) {
    common.printSkipMessage('IV len < 12 bytes unsupported in FIPS mode');
    continue;
  }

  const isCCM = /^aes-(128|192|256)-ccm$/.test(test.algo);

  let options;
  if (isCCM)
    options = { authTagLength: test.tag.length / 2 };

  const inputEncoding = test.plainIsHex ? 'hex' : 'ascii';

  let aadOptions;
  if (isCCM) {
    aadOptions = {
      plaintextLength: Buffer.from(test.plain, inputEncoding).length
    };
  }

  {
    const encrypt = crypto.createCipheriv(test.algo,
                                          Buffer.from(test.key, 'hex'),
                                          Buffer.from(test.iv, 'hex'),
                                          options);

    if (test.aad)
      encrypt.setAAD(Buffer.from(test.aad, 'hex'), aadOptions);

    let hex = encrypt.update(test.plain, inputEncoding, 'hex');
    hex += encrypt.final('hex');

    const auth_tag = encrypt.getAuthTag();
    // only test basic encryption run if output is marked as tampered.
    if (!test.tampered) {
      assert.strictEqual(hex, test.ct);
      assert.strictEqual(auth_tag.toString('hex'), test.tag);
    }
  }

  {
    if (isCCM && common.hasFipsCrypto) {
      assert.throws(() => {
        crypto.createDecipheriv(test.algo,
                                Buffer.from(test.key, 'hex'),
                                Buffer.from(test.iv, 'hex'),
                                options);
      }, errMessages.FIPS);
    } else {
      const decrypt = crypto.createDecipheriv(test.algo,
                                              Buffer.from(test.key, 'hex'),
                                              Buffer.from(test.iv, 'hex'),
                                              options);
      decrypt.setAuthTag(Buffer.from(test.tag, 'hex'));
      if (test.aad)
        decrypt.setAAD(Buffer.from(test.aad, 'hex'), aadOptions);

      const outputEncoding = test.plainIsHex ? 'hex' : 'ascii';

      let msg = decrypt.update(test.ct, 'hex', outputEncoding);
      if (!test.tampered) {
        msg += decrypt.final(outputEncoding);
        assert.strictEqual(msg, test.plain);
      } else {
        // assert that final throws if input data could not be verified!
        assert.throws(function() { decrypt.final('hex'); }, errMessages.auth);
      }
    }
  }

  if (test.password) {
    if (common.hasFipsCrypto) {
      assert.throws(() => { crypto.createCipher(test.algo, test.password); },
                    errMessages.FIPS);
    } else {
      const encrypt = crypto.createCipher(test.algo, test.password, options);
      if (test.aad)
        encrypt.setAAD(Buffer.from(test.aad, 'hex'), aadOptions);
      let hex = encrypt.update(test.plain, 'ascii', 'hex');
      hex += encrypt.final('hex');
      const auth_tag = encrypt.getAuthTag();
      // only test basic encryption run if output is marked as tampered.
      if (!test.tampered) {
        assert.strictEqual(hex, test.ct);
        assert.strictEqual(auth_tag.toString('hex'), test.tag);
      }
    }
  }

  if (test.password) {
    if (common.hasFipsCrypto) {
      assert.throws(() => { crypto.createDecipher(test.algo, test.password); },
                    errMessages.FIPS);
    } else {
      const decrypt = crypto.createDecipher(test.algo, test.password, options);
      decrypt.setAuthTag(Buffer.from(test.tag, 'hex'));
      if (test.aad)
        decrypt.setAAD(Buffer.from(test.aad, 'hex'), aadOptions);
      let msg = decrypt.update(test.ct, 'hex', 'ascii');
      if (!test.tampered) {
        msg += decrypt.final('ascii');
        assert.strictEqual(msg, test.plain);
      } else {
        // assert that final throws if input data could not be verified!
        assert.throws(function() { decrypt.final('ascii'); }, errMessages.auth);
      }
    }
  }

  {
    // trying to get tag before inputting all data:
    const encrypt = crypto.createCipheriv(test.algo,
                                          Buffer.from(test.key, 'hex'),
                                          Buffer.from(test.iv, 'hex'),
                                          options);
    encrypt.update('blah', 'ascii');
    assert.throws(function() { encrypt.getAuthTag(); }, errMessages.state);
  }

  {
    // trying to set tag on encryption object:
    const encrypt = crypto.createCipheriv(test.algo,
                                          Buffer.from(test.key, 'hex'),
                                          Buffer.from(test.iv, 'hex'),
                                          options);
    assert.throws(() => { encrypt.setAuthTag(Buffer.from(test.tag, 'hex')); },
                  errMessages.state);
  }

  {
    if (!isCCM || !common.hasFipsCrypto) {
      // trying to read tag from decryption object:
      const decrypt = crypto.createDecipheriv(test.algo,
                                              Buffer.from(test.key, 'hex'),
                                              Buffer.from(test.iv, 'hex'),
                                              options);
      assert.throws(function() { decrypt.getAuthTag(); }, errMessages.state);
    }
  }

  {
    // trying to create cipher with incorrect IV length
    assert.throws(function() {
      crypto.createCipheriv(
        test.algo,
        Buffer.from(test.key, 'hex'),
        Buffer.alloc(0)
      );
    }, errMessages.length);
  }
}

// Non-authenticating mode:
{
  const encrypt =
      crypto.createCipheriv('aes-128-cbc',
                            'ipxp9a6i1Mb4USb4',
                            '6fKjEjR3Vl30EUYC');
  encrypt.update('blah', 'ascii');
  encrypt.final();
  assert.throws(() => encrypt.getAuthTag(), errMessages.state);
  assert.throws(() => encrypt.setAAD(Buffer.from('123', 'ascii')),
                errMessages.state);
}

// GCM only supports specific authentication tag lengths, invalid lengths should
// produce warnings.
{
  for (const length of [0, 1, 2, 4, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17]) {
    const decrypt = crypto.createDecipheriv('aes-256-gcm',
                                            'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                                            'qkuZpJWCewa6Szih');
    decrypt.setAuthTag(Buffer.from('1'.repeat(length)));
  }
}

// Test that create(De|C)ipher(iv)? throws if the mode is CCM and an invalid
// authentication tag length has been specified.
{
  for (const authTagLength of [-1, true, false, NaN, 5.5]) {
    common.expectsError(() => {
      crypto.createCipheriv('aes-256-ccm',
                            'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                            'qkuZpJWCewa6S',
                            {
                              authTagLength
                            });
    }, {
      type: TypeError,
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${authTagLength}" is invalid for option ` +
               '"authTagLength"'
    });

    common.expectsError(() => {
      crypto.createDecipheriv('aes-256-ccm',
                              'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                              'qkuZpJWCewa6S',
                              {
                                authTagLength
                              });
    }, {
      type: TypeError,
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${authTagLength}" is invalid for option ` +
               '"authTagLength"'
    });

    if (!common.hasFipsCrypto) {
      common.expectsError(() => {
        crypto.createCipher('aes-256-ccm', 'bad password', { authTagLength });
      }, {
        type: TypeError,
        code: 'ERR_INVALID_OPT_VALUE',
        message: `The value "${authTagLength}" is invalid for option ` +
                 '"authTagLength"'
      });

      common.expectsError(() => {
        crypto.createDecipher('aes-256-ccm', 'bad password', { authTagLength });
      }, {
        type: TypeError,
        code: 'ERR_INVALID_OPT_VALUE',
        message: `The value "${authTagLength}" is invalid for option ` +
                 '"authTagLength"'
      });
    }
  }

  // The following values will not be caught by the JS layer and thus will not
  // use the default error codes.
  for (const authTagLength of [0, 1, 2, 3, 5, 7, 9, 11, 13, 15, 17, 18]) {
    assert.throws(() => {
      crypto.createCipheriv('aes-256-ccm',
                            'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                            'qkuZpJWCewa6S',
                            {
                              authTagLength
                            });
    }, errMessages.authTagLength);

    if (!common.hasFipsCrypto) {
      assert.throws(() => {
        crypto.createDecipheriv('aes-256-ccm',
                                'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                                'qkuZpJWCewa6S',
                                {
                                  authTagLength
                                });
      }, errMessages.authTagLength);

      assert.throws(() => {
        crypto.createCipher('aes-256-ccm', 'bad password', { authTagLength });
      }, errMessages.authTagLength);

      assert.throws(() => {
        crypto.createDecipher('aes-256-ccm', 'bad password', { authTagLength });
      }, errMessages.authTagLength);
    }
  }
}

// Test that create(De|C)ipher(iv)? throws if the mode is CCM and no
// authentication tag has been specified.
{
  assert.throws(() => {
    crypto.createCipheriv('aes-256-ccm',
                          'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                          'qkuZpJWCewa6S');
  }, /^Error: authTagLength required for aes-256-ccm$/);

  // CCM decryption and create(De|C)ipher are unsupported in FIPS mode.
  if (!common.hasFipsCrypto) {
    assert.throws(() => {
      crypto.createDecipheriv('aes-256-ccm',
                              'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                              'qkuZpJWCewa6S');
    }, /^Error: authTagLength required for aes-256-ccm$/);

    assert.throws(() => {
      crypto.createCipher('aes-256-ccm', 'very bad password');
    }, /^Error: authTagLength required for aes-256-ccm$/);

    assert.throws(() => {
      crypto.createDecipher('aes-256-ccm', 'very bad password');
    }, /^Error: authTagLength required for aes-256-ccm$/);
  }
}

// Test that setAAD throws if an invalid plaintext length has been specified.
{
  const cipher = crypto.createCipheriv('aes-256-ccm',
                                       'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                                       'qkuZpJWCewa6S',
                                       {
                                         authTagLength: 10
                                       });

  for (const plaintextLength of [-1, true, false, NaN, 5.5]) {
    common.expectsError(() => {
      cipher.setAAD(Buffer.from('0123456789', 'hex'), { plaintextLength });
    }, {
      type: TypeError,
      code: 'ERR_INVALID_OPT_VALUE',
      message: `The value "${plaintextLength}" is invalid for option ` +
               '"plaintextLength"'
    });
  }
}

// Test that setAAD and update throw if the plaintext is too long.
{
  for (const ivLength of [13, 12]) {
    const maxMessageSize = (1 << (8 * (15 - ivLength))) - 1;
    const key = 'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8';
    const cipher = () => crypto.createCipheriv('aes-256-ccm', key,
                                               '0'.repeat(ivLength),
                                               {
                                                 authTagLength: 10
                                               });

    assert.throws(() => {
      cipher().setAAD(Buffer.alloc(0), {
        plaintextLength: maxMessageSize + 1
      });
    }, /^Error: Message exceeds maximum size$/);

    const msg = Buffer.alloc(maxMessageSize + 1);
    assert.throws(() => {
      cipher().update(msg);
    }, /^Error: Message exceeds maximum size$/);

    const c = cipher();
    c.setAAD(Buffer.alloc(0), {
      plaintextLength: maxMessageSize
    });
    c.update(msg.slice(1));
  }
}

// Test that setAAD throws if the mode is CCM and the plaintext length has not
// been specified.
{
  assert.throws(() => {
    const cipher = crypto.createCipheriv('aes-256-ccm',
                                         'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                                         'qkuZpJWCewa6S',
                                         {
                                           authTagLength: 10
                                         });
    cipher.setAAD(Buffer.from('0123456789', 'hex'));
  }, /^Error: plaintextLength required for CCM mode with AAD$/);

  if (!common.hasFipsCrypto) {
    assert.throws(() => {
      const cipher = crypto.createDecipheriv('aes-256-ccm',
                                             'FxLKsqdmv0E9xrQhp0b1ZgI0K7JFZJM8',
                                             'qkuZpJWCewa6S',
                                             {
                                               authTagLength: 10
                                             });
      cipher.setAAD(Buffer.from('0123456789', 'hex'));
    }, /^Error: plaintextLength required for CCM mode with AAD$/);
  }
}

// Test that setAAD throws in CCM mode when no authentication tag is provided.
{
  if (!common.hasFipsCrypto) {
    const key = Buffer.from('1ed2233fa2223ef5d7df08546049406c', 'hex');
    const iv = Buffer.from('7305220bca40d4c90e1791e9', 'hex');
    const ct = Buffer.from('8beba09d4d4d861f957d51c0794f4abf8030848e', 'hex');
    const decrypt = crypto.createDecipheriv('aes-128-ccm', key, iv, {
      authTagLength: 10
    });
    // Normally, we would do this:
    // decrypt.setAuthTag(Buffer.from('0d9bcd142a94caf3d1dd', 'hex'));
    assert.throws(() => {
      decrypt.setAAD(Buffer.from('63616c76696e', 'hex'), {
        plaintextLength: ct.length
      });
    }, errMessages.state);
  }
}

// Test that setAuthTag does not throw in GCM mode when called after setAAD.
{
  const key = Buffer.from('1ed2233fa2223ef5d7df08546049406c', 'hex');
  const iv = Buffer.from('579d9dfde9cd93d743da1ceaeebb86e4', 'hex');
  const decrypt = crypto.createDecipheriv('aes-128-gcm', key, iv);
  decrypt.setAAD(Buffer.from('0123456789', 'hex'));
  decrypt.setAuthTag(Buffer.from('1bb9253e250b8069cde97151d7ef32d9', 'hex'));
  assert.strictEqual(decrypt.update('807022', 'hex', 'hex'), 'abcdef');
  assert.strictEqual(decrypt.final('hex'), '');
}
