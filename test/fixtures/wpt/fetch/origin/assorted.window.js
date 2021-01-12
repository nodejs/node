// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

const origins = get_host_info();

promise_test(async function () {
  const stash = token(),
        redirectPath = "/fetch/origin/resources/redirect-and-stash.py";

  // Cross-origin -> same-origin will result in setting the tainted origin flag for the second
  // request.
  let url = origins.HTTP_ORIGIN + redirectPath + "?stash=" + stash;
  url = origins.HTTP_REMOTE_ORIGIN + redirectPath + "?stash=" + stash + "&location=" + encodeURIComponent(url);

  await fetch(url, { mode: "no-cors", method: "POST" });

  const json = await (await fetch(redirectPath + "?dump&stash=" + stash)).json();

  assert_equals(json[0], origins.HTTP_ORIGIN);
  assert_equals(json[1], "null");
}, "Origin header and 308 redirect");

promise_test(async function () {
  const stash = token(),
        redirectPath = "/fetch/origin/resources/redirect-and-stash.py";

  let url = origins.HTTP_ORIGIN + redirectPath + "?stash=" + stash;
  url = origins.HTTP_REMOTE_ORIGIN + redirectPath + "?stash=" + stash + "&location=" + encodeURIComponent(url);

  await new Promise(resolve => {
    const frame = document.createElement("iframe");
    frame.src = url;
    frame.onload = () => {
      resolve();
      frame.remove();
    }
    document.body.appendChild(frame);
  });

  const json = await (await fetch(redirectPath + "?dump&stash=" + stash)).json();

  assert_equals(json[0], "no Origin header");
  assert_equals(json[1], "no Origin header");
}, "Origin header and GET navigation");

promise_test(async function () {
  const stash = token(),
        redirectPath = "/fetch/origin/resources/redirect-and-stash.py";

  let url = origins.HTTP_ORIGIN + redirectPath + "?stash=" + stash;
  url = origins.HTTP_REMOTE_ORIGIN + redirectPath + "?stash=" + stash + "&location=" + encodeURIComponent(url);

  await new Promise(resolve => {
    const frame = document.createElement("iframe");
    self.addEventListener("message", e => {
      if (e.data === "loaded") {
        resolve();
        frame.remove();
      }
    }, { once: true });
    frame.onload = () => {
      const doc = frame.contentDocument,
            form = doc.body.appendChild(doc.createElement("form")),
            submit = form.appendChild(doc.createElement("input"));
      form.action = url;
      form.method = "POST";
      submit.type = "submit";
      submit.click();
    }
    document.body.appendChild(frame);
  });

  const json = await (await fetch(redirectPath + "?dump&stash=" + stash)).json();

  assert_equals(json[0], origins.HTTP_ORIGIN);
  assert_equals(json[1], "null");
}, "Origin header and POST navigation");

function navigationReferrerPolicy(referrerPolicy, destination, expectedOrigin) {
  return async function () {
    const stash = token();
    const referrerPolicyPath = "/fetch/origin/resources/referrer-policy.py";
    const redirectPath = "/fetch/origin/resources/redirect-and-stash.py";

    let postUrl =
            (destination === "same-origin" ? origins.HTTP_ORIGIN
                                           : origins.HTTP_REMOTE_ORIGIN) +
            redirectPath + "?stash=" + stash;

    await new Promise(resolve => {
      const frame = document.createElement("iframe");
      document.body.appendChild(frame);
      frame.src = origins.HTTP_ORIGIN + referrerPolicyPath +
                  "?referrerPolicy=" + referrerPolicy;
      self.addEventListener("message", function listener(e) {
        if (e.data === "loaded") {
          resolve();
          frame.remove();
          self.removeEventListener("message", listener);
        } else if (e.data === "action") {
          const doc = frame.contentDocument,
                form = doc.body.appendChild(doc.createElement("form")),
                submit = form.appendChild(doc.createElement("input"));
          form.action = postUrl;
          form.method = "POST";
          submit.type = "submit";
          submit.click();
        }
      });
    });

    const json = await (await fetch(redirectPath + "?dump&stash=" + stash)).json();

    assert_equals(json[0], expectedOrigin);
  };
}

function fetchReferrerPolicy(referrerPolicy, destination, fetchMode, expectedOrigin, httpMethod) {
  return async function () {
    const stash = token();
    const redirectPath = "/fetch/origin/resources/redirect-and-stash.py";

    let fetchUrl =
        (destination === "same-origin" ? origins.HTTP_ORIGIN
                                       : origins.HTTP_REMOTE_ORIGIN) +
        redirectPath + "?stash=" + stash;

    await fetch(fetchUrl, { mode: fetchMode, method: httpMethod , "referrerPolicy": referrerPolicy});

    const json = await (await fetch(redirectPath + "?dump&stash=" + stash)).json();

    assert_equals(json[0], expectedOrigin);
  };
}

function referrerPolicyTestString(referrerPolicy, method, destination) {
  return "Origin header and " + method + " " + destination + " with Referrer-Policy " +
         referrerPolicy;
}

[
  {
    "policy": "no-referrer",
    "expectedOriginForSameOrigin": "null",
    "expectedOriginForCrossOrigin": "null"
  },
  {
    "policy": "same-origin",
    "expectedOriginForSameOrigin": origins.HTTP_ORIGIN,
    "expectedOriginForCrossOrigin": "null"
  },
  {
    "policy": "origin-when-cross-origin",
    "expectedOriginForSameOrigin": origins.HTTP_ORIGIN,
    "expectedOriginForCrossOrigin": origins.HTTP_ORIGIN
  },
  {
    "policy": "no-referrer-when-downgrade",
    "expectedOriginForSameOrigin": origins.HTTP_ORIGIN,
    "expectedOriginForCrossOrigin": origins.HTTP_ORIGIN
  },
  {
    "policy": "unsafe-url",
    "expectedOriginForSameOrigin": origins.HTTP_ORIGIN,
    "expectedOriginForCrossOrigin": origins.HTTP_ORIGIN
  },
].forEach(testObj => {
  [
    {
      "name": "same-origin",
      "expectedOrigin": testObj.expectedOriginForSameOrigin
    },
    {
      "name": "cross-origin",
      "expectedOrigin": testObj.expectedOriginForCrossOrigin
    }
  ].forEach(destination => {
    // Test form POST navigation
    promise_test(navigationReferrerPolicy(testObj.policy,
                                          destination.name,
                                          destination.expectedOrigin),
                 referrerPolicyTestString(testObj.policy, "POST",
                                          destination.name + " navigation"));
    // Test fetch
    promise_test(fetchReferrerPolicy(testObj.policy,
                                     destination.name,
                                     "no-cors",
                                     destination.expectedOrigin,
                                     "POST"),
                 referrerPolicyTestString(testObj.policy, "POST",
                                          destination.name + " fetch no-cors mode"));

    // Test cors mode POST
    promise_test(fetchReferrerPolicy(testObj.policy,
                                     destination.name,
                                     "cors",
                                     (destination.name == "same-origin") ? destination.expectedOrigin : origins.HTTP_ORIGIN,
                                     "POST"),
                 referrerPolicyTestString(testObj.policy, "POST",
                                          destination.name + " fetch cors mode"));

    // Test cors mode GET
    promise_test(fetchReferrerPolicy(testObj.policy,
                                     destination.name,
                                     "cors",
                                     (destination.name == "same-origin") ? "no Origin header" : origins.HTTP_ORIGIN,
                                     "GET"),
                 referrerPolicyTestString(testObj.policy, "GET",
                                          destination.name + " fetch cors mode"));
  });
});
