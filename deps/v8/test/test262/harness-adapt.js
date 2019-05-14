// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

function fnGlobalObject() { return (function() { return this; })(); }

var ES5Harness = (function() {
  var currentTest = {};
  var $this = this;

  function Test262Error(id, path, description, codeString,
                        preconditionString, result, error) {
    this.id = id;
    this.path = path;
    this.description = description;
    this.result = result;
    this.error = error;
    this.code = codeString;
    this.pre = preconditionString;
  }

  Test262Error.prototype.toString = function() {
    return this.result + " " + this.error;
  }

  function registerTest(test) {
    if (!(test.precondition && !test.precondition())) {
      var error;
      try {
        var res = test.test.call($this);
      } catch(e) {
        res = 'fail';
        error = e;
      }
      var retVal = /^s/i.test(test.id)
          ? (res === true || typeof res == 'undefined' ? 'pass' : 'fail')
          : (res === true ? 'pass' : 'fail');

      if (retVal != 'pass') {
         var precondition = (test.precondition !== undefined)
             ? test.precondition.toString()
             : '';

         throw new Test262Error(
            test.id,
            test.path,
            test.description,
            test.test.toString(),
            precondition,
            retVal,
            error);
      }
    }
  }

  return {
    registerTest: registerTest
  }
})();

function $DONE(arg){
    if (arg) {
        print('FAILED! Error: ' + arg);
        quit(1);
    }

    quit(0);
};

function RealmOperators(realm) {
  let $262 = {
    evalScript(script) {
      return Realm.eval(realm, script);
    },
    createRealm() {
      return RealmOperators(Realm.createAllowCrossRealmAccess());
    },
    global: Realm.eval(realm, 'this')
  };
  $262.global.$262 = $262;
  return $262;
}

var $262 = RealmOperators(Realm.current());
