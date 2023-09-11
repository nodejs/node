module.exports = [
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

  // OCB test cases from RFC7253
  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221100',
    plain: '',
    ct: '',
    tag: '785407bfffc8ad9edcc5520ac9111ee6'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221101',
    plain: '0001020304050607',
    plainIsHex: true,
    aad: '0001020304050607',
    ct: '6820b3657b6f615a',
    tag: '5725bda0d3b4eb3a257c9af1f8f03009'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221102',
    plain: '',
    aad: '0001020304050607',
    ct: '',
    tag: '81017f8203f081277152fade694a0a00'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221103',
    plain: '0001020304050607',
    plainIsHex: true,
    ct: '45dd69f8f5aae724',
    tag: '14054cd1f35d82760b2cd00d2f99bfa9'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221104',
    plain: '000102030405060708090a0b0c0d0e0f',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f',
    ct: '571d535b60b277188be5147170a9a22c',
    tag: '3ad7a4ff3835b8c5701c1ccec8fc3358'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221105',
    plain: '',
    aad: '000102030405060708090a0b0c0d0e0f',
    ct: '',
    tag: '8cf761b6902ef764462ad86498ca6b97'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221106',
    plain: '000102030405060708090a0b0c0d0e0f',
    plainIsHex: true,
    ct: '5ce88ec2e0692706a915c00aeb8b2396',
    tag: 'f40e1c743f52436bdf06d8fa1eca343d'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221107',
    plain: '000102030405060708090a0b0c0d0e0f1011121314151617',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f1011121314151617',
    ct: '1ca2207308c87c010756104d8840ce1952f09673a448a122',
    tag: 'c92c62241051f57356d7f3c90bb0e07f'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221108',
    plain: '',
    aad: '000102030405060708090a0b0c0d0e0f1011121314151617',
    ct: '',
    tag: '6dc225a071fc1b9f7c69f93b0f1e10de'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa99887766554433221109',
    plain: '000102030405060708090a0b0c0d0e0f1011121314151617',
    plainIsHex: true,
    ct: '221bd0de7fa6fe993eccd769460a0af2d6cded0c395b1c3c',
    tag: 'e725f32494b9f914d85c0b1eb38357ff'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110a',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    ct: 'bd6f6c496201c69296c11efd138a467abd3c707924b964deaffc40319af5a485',
    tag: '40fbba186c5553c68ad9f592a79a4240'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110b',
    plain: '',
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    ct: '',
    tag: 'fe80690bee8a485d11f32965bc9d2a32'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110c',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f',
    plainIsHex: true,
    ct: '2942bfc773bda23cabc6acfd9bfd5835bd300f0973792ef46040c53f1432bcdf',
    tag: 'b5e1dde3bc18a5f840b52e653444d5df'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110d',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
           '2021222324252627',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20' +
         '21222324252627',
    ct: 'd5ca91748410c1751ff8a2f618255b68a0a12e093ff454606e59f9c1d0ddc54b65e8' +
        '628e568bad7a',
    tag: 'ed07ba06a4a69483a7035490c5769e60'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110e',
    plain: '',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20' +
         '21222324252627',
    ct: '',
    tag: 'c5cd9d1850c141e358649994ee701b68'
  },

  {
    algo: 'aes-128-ocb',
    key: '000102030405060708090a0b0c0d0e0f',
    iv: 'bbaa9988776655443322110f',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
           '2021222324252627',
    plainIsHex: true,
    ct: '4412923493c57d5de0d700f753cce0d1d2d95060122e9f15a5ddbfc5787e50b5cc55' +
        'ee507bcb084e',
    tag: '479ad363ac366b95a98ca5f3000b1479'
  },

  {
    algo: 'aes-128-ocb',
    key: '0f0e0d0c0b0a09080706050403020100',
    iv: 'bbaa9988776655443322110d',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
           '2021222324252627',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20' +
         '21222324252627',
    ct: '1792a4e31e0755fb03e31b22116e6c2ddf9efd6e33d536f1a0124b0a55bae884ed93' +
        '481529c76b6a',
    tag: 'd0c515f4d1cdd4fdac4f02aa'
  },

  {
    algo: 'aes-128-ocb',
    key: '0f0e0d0c0b0a09080706050403020100',
    iv: 'bbaa9988776655443322110d',
    plain: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f' +
           '2021222324252627',
    plainIsHex: true,
    aad: '000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20' +
         '21222324252627',
    ct: '1792a4e31e0755fb03e31b22116e6c2ddf9efd6e33d536f1a0124b0a55bae884ed93' +
        '481529c76b6a',
    tag: 'd0c515f4d1cdd4fdac4f02ab',
    tampered: true
  },

  // Test case from rfc7539 section 2.8.2
  { algo: 'chacha20-poly1305',
    key: '808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f',
    iv: '070000004041424344454647',
    plain: '4c616469657320616e642047656e746c656d656e206f662074686520636c6173' +
           '73206f66202739393a204966204920636f756c64206f6666657220796f75206f' +
           '6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73' +
           '637265656e20776f756c642062652069742e',
    plainIsHex: true,
    aad: '50515253c0c1c2c3c4c5c6c7',
    ct: 'd31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5' +
        'a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e06' +
        '0b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fa' +
        'b324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d265' +
        '86cec64b6116',
    tag: '1ae10b594f09e26a7e902ecbd0600691',
    tampered: false
  },

  { algo: 'chacha20-poly1305',
    key: '808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f',
    iv: '070000004041424344454647',
    plain: '4c616469657320616e642047656e746c656d656e206f662074686520636c6173' +
           '73206f66202739393a204966204920636f756c64206f6666657220796f75206f' +
           '6e6c79206f6e652074697020666f7220746865206675747572652c2073756e73' +
           '637265656e20776f756c642062652069742e',
    plainIsHex: true,
    aad: '50515253c0c1c2c3c4c5c6c7',
    ct: 'd31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5' +
        'a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e06' +
        '0b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fa' +
        'b324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d265' +
        '86cec64b6116',
    tag: '1ae10b594f09e26a7e902ecbd0600692',
    tampered: true
  }
];
