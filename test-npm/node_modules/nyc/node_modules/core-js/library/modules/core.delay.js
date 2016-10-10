var global  = require('./_global')
  , core    = require('./_core')
  , $export = require('./_export')
  , partial = require('./_partial');
// https://esdiscuss.org/topic/promise-returning-delay-function
$export($export.G + $export.F, {
  delay: function delay(time){
    return new (core.Promise || global.Promise)(function(resolve){
      setTimeout(partial.call(resolve, true), time);
    });
  }
});