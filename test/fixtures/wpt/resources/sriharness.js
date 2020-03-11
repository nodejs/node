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

function set_extra_attributes(element, attrs) {
  // Apply the rest of the attributes, if any.
  for (const [attr_name, attr_val] of Object.entries(attrs)) {
    element[attr_name] = attr_val;
  }
}

function buildElementFromDestination(resource_url, destination, attrs) {
  // Assert: |destination| is a valid destination.
  let element;

  // The below switch is responsible for:
  //   1. Creating the correct subresource element
  //   2. Setting said element's href, src, or fetch-instigating property
  //      appropriately.
  switch (destination) {
    case "script":
      element = document.createElement(destination);
      set_extra_attributes(element, attrs);
      element.src = resource_url;
      break;
    case "style":
      element = document.createElement('link');
      set_extra_attributes(element, attrs);
      element.rel = 'stylesheet';
      element.href = resource_url;
      break;
    case "image":
      element = document.createElement('img');
      set_extra_attributes(element, attrs);
      element.src = resource_url;
      break;
    default:
      assert_unreached("INVALID DESTINATION");
  }

  return element;
}

const SRIPreloadTest = (preload_sri_success, subresource_sri_success, name,
                        destination, resource_url, link_attrs,
                        subresource_attrs) => {
  const test = async_test(name);
  const link = document.createElement('link');

  // Early-fail in UAs that do not support `preload` links.
  test.step_func(() => {
    assert_true(link.relList.supports('preload'),
      "This test is automatically failing because the browser does not" +
      "support `preload` links.");
  })();

  // Build up the link.
  link.rel = 'preload';
  link.as = destination;
  link.href = resource_url;
  for (const [attr_name, attr_val] of Object.entries(link_attrs)) {
    link[attr_name] = attr_val; // This may override `rel` to modulepreload.
  }

  // Preload + subresource success and failure loading functions.
  const valid_preload_failed = test.step_func(() =>
    { assert_unreached("Valid preload fired error handler.") });
  const invalid_preload_succeeded = test.step_func(() =>
    { assert_unreached("Invalid preload load succeeded.") });
  const valid_subresource_failed = test.step_func(() =>
    { assert_unreached("Valid subresource fired error handler.") });
  const invalid_subresource_succeeded = test.step_func(() =>
    { assert_unreached("Invalid subresource load succeeded.") });
  const subresource_pass = test.step_func(() => { test.done(); });
  const preload_pass = test.step_func(() => {
    const subresource_element = buildElementFromDestination(
      resource_url,
      destination,
      subresource_attrs
    );

    if (subresource_sri_success) {
      subresource_element.onload = subresource_pass;
      subresource_element.onerror = valid_subresource_failed;
    } else {
      subresource_element.onload = invalid_subresource_succeeded;
      subresource_element.onerror = subresource_pass;
    }

    document.body.append(subresource_element);
  });

  if (preload_sri_success) {
    link.onload = preload_pass;
    link.onerror = valid_preload_failed;
  } else {
    link.onload = invalid_preload_succeeded;
    link.onerror = preload_pass;
  }

  document.head.append(link);
}

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

    // The link relation is guaranteed to not be "preload" or "modulepreload".
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

