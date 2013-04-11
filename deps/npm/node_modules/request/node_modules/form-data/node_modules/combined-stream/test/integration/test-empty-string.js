var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;
var util = require('util');
var Stream = require('stream').Stream;

var s = CombinedStream.create();


function StringStream(){
  this.writable=true;
  this.str=""
}
util.inherits(StringStream,Stream);

StringStream.prototype.write=function(chunk,encoding){
  this.str+=chunk.toString();
  this.emit('data',chunk);
}

StringStream.prototype.end=function(chunk,encoding){
  this.emit('end');
}

StringStream.prototype.toString=function(){
  return this.str;
}


s.append("foo.");
s.append("");
s.append("bar");

var ss = new StringStream();

s.pipe(ss);
s.resume();

assert.equal(ss.toString(),"foo.bar");
