require("../common");
var fs = require('fs');
var sys = require('sys');

var have_openssl;
try {
  var crypto = require('crypto');
  have_openssl=true;
} catch (e) {
  have_openssl=false;
  puts("Not compiled with OPENSSL support.");
  process.exit();
} 

var caPem = fs.readFileSync(fixturesDir+"/test_ca.pem");
var certPem = fs.readFileSync(fixturesDir+"/test_cert.pem");
var keyPem = fs.readFileSync(fixturesDir+"/test_key.pem");

var credentials = crypto.createCredentials({key:keyPem, cert:certPem, ca:caPem});

// Test HMAC
//var h1 = (new crypto.Hmac).init("sha1", "Node").update("some data").update("to hmac").digest("hex");
var h1 = crypto.createHmac("sha1", "Node").update("some data").update("to hmac").digest("hex");
assert.equal(h1, '19fd6e1ba73d9ed2224dd5094a71babe85d9a892', "test HMAC");

// Test hashing
var a0 = crypto.createHash("sha1").update("Test123").digest("hex");
var a1 = crypto.createHash("md5").update("Test123").digest("binary");
var a2=  crypto.createHash("sha256").update("Test123").digest("base64");
var a3 = crypto.createHash("sha512").update("Test123").digest(); // binary

// Test multiple updates to same hash
var h1 = crypto.createHash("sha1").update("Test123").digest("hex");
var h2 = (new crypto.Hash).init("sha1").update("Test").update("123").digest("hex");
assert.equal(h1, h2, "multipled updates");


// Test signing and verifying
var s1 = crypto.createSign("RSA-SHA1").update("Test123").sign(keyPem, "base64");
var verified = crypto.createVerify("RSA-SHA1").update("Test").update("123").verify(certPem, s1, "base64");
assert.ok(verified, "sign and verify (base 64)");

var s2 = crypto.createSign("RSA-SHA256").update("Test123").sign(keyPem); // binary
var verified = crypto.createVerify("RSA-SHA256").update("Test").update("123").verify(certPem, s2); // binary
assert.ok(verified, "sign and verify (binary)");

// Test encryption and decryption
var plaintext="Keep this a secret? No! Tell everyone about node.js!";

var cipher=crypto.createCipher("aes192", "MySecretKey123");
var ciph=cipher.update(plaintext, 'utf8', 'hex'); // encrypt plaintext which is in utf8 format to a ciphertext which will be in hex
ciph+=cipher.final('hex'); // Only use binary or hex, not base64.

var decipher=crypto.createDecipher("aes192", "MySecretKey123");
var txt = decipher.update(ciph, 'hex', 'utf8');
txt += decipher.final('utf8');
assert.equal(txt, plaintext, "encryption and decryption");

// Test encyrption and decryption with explicit key and iv
var encryption_key='0123456789abcd0123456789';
var iv = '12345678';

var cipher=crypto.createCipheriv("des-ede3-cbc", encryption_key, iv);

var ciph=cipher.update(plaintext, 'utf8', 'hex'); 
ciph+=cipher.final('hex');

var decipher=crypto.createDecipheriv("des-ede3-cbc",encryption_key,iv);
var txt = decipher.update(ciph, 'hex', 'utf8');
txt += decipher.final('utf8');
assert.equal(txt, plaintext, "encryption and decryption with key and iv");

