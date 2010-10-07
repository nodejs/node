
var common = require("../common");
var path = require("path");
var assert = common.assert;
var bigFile = path.join(common.tmpDir, "bigFile");
var fs = require("fs");
var cp = require("child_process");

console.error("Note: This test requires the dd, base64, and shasum programs.");

fs.stat(bigFile, function (er, s) {
  if (er || s.size < 1024*1024*128) {
    makeBigFile();
  } else {
    doEncoding();
  }
})

function makeBigFile () {
  console.error("making bigFile");
  cp.exec("dd if=/dev/zero of="+bigFile+" bs=128m count=1", function (er) {
    if (er) {
      console.error("Failed to create "+bigFile);
      throw er;
    }
    doEncoding();
  });
}

function doEncoding () {
  // encode with node.
  console.error("doing encoding");
  fs.readFile(bigFile, function (er, data) {
    if (er) throw er;
    var b64 = new Buffer(data.toString("base64"));
    fs.writeFile(bigFile+"-enc-node", b64, encDone);
  });
  // encode with base64
  cp.exec("base64 <"+bigFile+" >"+bigFile+"-enc-native", encDone);

  var encCalls = 2;
  function encDone (er) {
    if (er) {
      console.error("failed to encode");
      throw er;
    }
    if (-- encCalls > 0) return;
    doDecoding()
  }
}

function doDecoding () {
  console.error("doing decoding");
  // decode both files both ways
  decodeNode(  bigFile+"-enc-node"  , decDone);
  decodeNode(  bigFile+"-enc-native", decDone);
  decodeNative(bigFile+"-enc-node"  , decDone);
  decodeNative(bigFile+"-enc-native", decDone);
  
  var decCalls = 4;
  function decDone (er) {
    if (er) {
      console.error("failed to decode");
      throw er;
    }
    if (-- decCalls > 0) return;
    doShasum();
  }
}

function decodeNode (f, cb) {
  // decode with node.
  console.error("decode "+path.basename(f)+" with node");
  fs.readFile(f, function (er, data) {
    if (er) throw er;
    var dec = new Buffer(data.toString(), "base64");
    fs.writeFile(f+"-dec-node", dec, cb);
  });
}

function decodeNative (f, cb) {
  console.error("decode "+path.basename(f)+" natively");
  cp.exec("base64 -d <"+f+" >"+f+"-dec-native", cb);
}

// check the shasums of all decoded files match the original.
function doShasum () {
  console.error("do shasums");
  // make sure to get the original first.
  shaFile(bigFile, function (file, out) {
    checkSha(file, out);
    shaFile(bigFile+"-enc-node-dec-node"    , checkSha);
    shaFile(bigFile+"-enc-native-dec-node"  , checkSha);
    shaFile(bigFile+"-enc-node-dec-native"  , checkSha);
    shaFile(bigFile+"-enc-native-dec-native", checkSha);
  });
}

function shaFile (file, cb) {
  console.error("shasum "+path.basename(file));
  cp.exec("shasum "+file, function (er, out) {
    if (er) {
      console.error("failed to shasum "+path.basename(file));
      throw er;
    }
    cb(file, out);
  });
}

var sha = null;
var fail = null;
var checks = 5;

function checkSha (file, out) {
  var s = out.toString().match(/^([a-fA-F0-9]{40})/);
  if (!s) {
    console.error("bad sha: "+out);
    throw new Error("fail");
  }
  s = s[0];
  console.error(s+" "+path.basename(file));
  if (sha === null) {
    sha = s;
  } else {
    // assert.equal(sha, s, file+" doesn't match shasum "+s);
    if (sha !== s) {
      console.error(path.basename(file)+" sha doesn't match")
      fail = fail || new Error("Some shasums didn't match");
    }
  }
  if (-- checks === 0) {
    if (fail) throw fail;
  }
}

