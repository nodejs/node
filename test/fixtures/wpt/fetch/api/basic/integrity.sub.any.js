// META: global=window,dedicatedworker,sharedworker
// META: script=../resources/utils.js

function integrity(desc, url, integrity, initRequestMode, shouldPass) {
  var fetchRequestInit = {'integrity': integrity}
  if (!!initRequestMode && initRequestMode !== "") {
    fetchRequestInit.mode = initRequestMode;
  }

  if (shouldPass) {
    promise_test(function(test) {
      return fetch(url, fetchRequestInit).then(function(resp) {
        if (initRequestMode !== "no-cors") {
          assert_equals(resp.status, 200, "Response's status is 200");
        } else {
          assert_equals(resp.status, 0, "Opaque response's status is 0");
          assert_equals(resp.type, "opaque");
        }
      });
    }, desc);
  } else {
    promise_test(function(test) {
      return promise_rejects_js(test, TypeError, fetch(url, fetchRequestInit));
    }, desc);
  }
}

const topSha256 = "sha256-KHIDZcXnR2oBHk9DrAA+5fFiR6JjudYjqoXtMR1zvzk=";
const topSha384 = "sha384-MgZYnnAzPM/MjhqfOIMfQK5qcFvGZsGLzx4Phd7/A8fHTqqLqXqKo8cNzY3xEPTL";
const topSha512 = "sha512-D6yns0qxG0E7+TwkevZ4Jt5t7Iy3ugmAajG/dlf6Pado1JqTyneKXICDiqFIkLMRExgtvg8PlxbKTkYfRejSOg==";
const invalidSha256 = "sha256-dKUcPOn/AlUjWIwcHeHNqYXPlvyGiq+2dWOdFcE+24I=";
const invalidSha512 = "sha512-oUceBRNxPxnY60g/VtPCj2syT4wo4EZh2CgYdWy9veW8+OsReTXoh7dizMGZafvx9+QhMS39L/gIkxnPIn41Zg==";

const path = dirname(location.pathname) + RESOURCES_DIR + "top.txt";
const url = path;
const corsUrl =
  `http://{{host}}:{{ports[http][1]}}${path}?pipe=header(Access-Control-Allow-Origin,*)`;
const corsUrl2 = `https://{{host}}:{{ports[https][0]}}${path}`

integrity("Empty string integrity", url, "", /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("SHA-256 integrity", url, topSha256, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("SHA-384 integrity", url, topSha384, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("SHA-512 integrity", url, topSha512, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("Invalid integrity", url, invalidSha256,
          /* initRequestMode */ undefined, /* shouldPass */  false);
integrity("Multiple integrities: valid stronger than invalid", url,
          invalidSha256 + " " + topSha384, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("Multiple integrities: invalid stronger than valid",
          url, invalidSha512 + " " + topSha384, /* initRequestMode */ undefined,
          /* shouldPass */ false);
integrity("Multiple integrities: invalid as strong as valid", url,
          invalidSha512 + " " + topSha512, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("Multiple integrities: both are valid", url,
          topSha384 + " " + topSha512, /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("Multiple integrities: both are invalid", url,
          invalidSha256 + " " + invalidSha512, /* initRequestMode */ undefined,
          /* shouldPass */ false);
integrity("CORS empty integrity", corsUrl, "", /* initRequestMode */ undefined,
          /* shouldPass */ true);
integrity("CORS SHA-512 integrity", corsUrl, topSha512,
          /* initRequestMode */ undefined, /* shouldPass */ true);
integrity("CORS invalid integrity", corsUrl, invalidSha512,
          /* initRequestMode */ undefined, /* shouldPass */ false);

integrity("Empty string integrity for opaque response", corsUrl2, "",
          /* initRequestMode */ "no-cors", /* shouldPass */ true);
integrity("SHA-* integrity for opaque response", corsUrl2, topSha512,
          /* initRequestMode */ "no-cors", /* shouldPass */ false);

done();
