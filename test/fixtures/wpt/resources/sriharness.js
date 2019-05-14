var SRIScriptTest = function(pass, name, src, integrityValue, crossoriginValue, nonce) {
    this.pass = pass;
    this.name = "Script: " + name;
    this.src = src;
    this.integrityValue = integrityValue;
    this.crossoriginValue = crossoriginValue;
    this.nonce = nonce;
}

SRIScriptTest.prototype.execute = function() {
    var test = async_test(this.name);
    var e = document.createElement("script");
    e.src = this.src;
    e.setAttribute("integrity", this.integrityValue);
    if(this.crossoriginValue) {
        e.setAttribute("crossorigin", this.crossoriginValue);
    }
    if(this.nonce) {
      e.setAttribute("nonce", this.nonce);
    }
    if(this.pass) {
        e.addEventListener("load", function() {test.done()});
        e.addEventListener("error", function() {
            test.step(function(){ assert_unreached("Good load fired error handler.") })
        });
    } else {
       e.addEventListener("load", function() {
            test.step(function() { assert_unreached("Bad load succeeded.") })
        });
       e.addEventListener("error", function() {test.done()});
    }
    document.body.appendChild(e);
};

// <link> tests
// Style tests must be done synchronously because they rely on the presence
// and absence of global style, which can affect later tests. Thus, instead
// of executing them one at a time, the style tests are implemented as a
// queue that builds up a list of tests, and then executes them one at a
// time.
var SRIStyleTest = function(queue, pass, name, attrs, customCallback, altPassValue) {
    this.pass = pass;
    this.name = "Style: " + name;
    this.customCallback = customCallback || function () {};
    this.attrs = attrs || {};
    this.passValue = altPassValue || "rgb(255, 255, 0)";

    this.test = async_test(this.name);

    this.queue = queue;
    this.queue.push(this);
}

SRIStyleTest.prototype.execute = function() {
  var that = this;
    var container = document.getElementById("container");
    while (container.hasChildNodes()) {
      container.removeChild(container.firstChild);
    }

    var test = this.test;

    var div = document.createElement("div");
    div.className = "testdiv";
    var e = document.createElement("link");
    this.attrs.rel = this.attrs.rel || "stylesheet";
    for (var key in this.attrs) {
        if (this.attrs.hasOwnProperty(key)) {
            e.setAttribute(key, this.attrs[key]);
        }
    }

    if(this.pass) {
        e.addEventListener("load", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_equals(background, that.passValue);
                test.done();
            });
        });
        e.addEventListener("error", function() {
            test.step(function(){ assert_unreached("Good load fired error handler.") })
        });
    } else {
        e.addEventListener("load", function() {
             test.step(function() { assert_unreached("Bad load succeeded.") })
         });
        e.addEventListener("error", function() {
            test.step(function() {
                var background = window.getComputedStyle(div, null).getPropertyValue("background-color");
                assert_not_equals(background, that.passValue);
                test.done();
            });
        });
    }
    container.appendChild(div);
    container.appendChild(e);
    this.customCallback(e, container);
};
