process.mixin(require('./common'));

var promise = new process.Promise();
var cancelled = false;
promise.addCancelback(function(){
  if(cancelled){
    assertUnreachable("promise should not cancel more than once");
  }
  cancelled = true;
});
promise.cancel();
promise.cancel();
