let destination = location;

if (location.search == "?cross-site") {
    const https = destination.protocol.startsWith("https");
    destination = get_host_info()[https ? 'HTTPS_NOTSAMESITE_ORIGIN' : 'HTTP_NOTSAMESITE_ORIGIN'];
} else if (location.search == "?crossorigin") {
    destination =  get_host_info().REMOTE_ORIGIN;
}

const pre_navigate_url =
  new URL("/resource-timing/resources/document-that-navigates.html",
    destination).href;
const post_navigate_url =
  new URL("/resource-timing/resources/document-navigated.html",
    destination).href;
const pre_refresh_url =
  new URL("/resource-timing/resources/document-that-refreshes.html",
    destination).href;
const post_refresh_url =
  new URL("/resource-timing/resources/document-refreshed.html",
    destination).href;

const setup_navigate_or_refresh = (type, pre, post) => {
  const verify_document_navigate_not_observable = () => {
    const entries = performance.getEntriesByType("resource");
    let found_first_document = false;
    for (entry of entries) {
      if (entry.name == pre) {
        found_first_document = true;
      }
      if (entry.name == post) {
        opener.postMessage(`FAIL - ${type} document should not be observable`,
          `*`);
        return;
      }
    }
    if (!found_first_document) {
      opener.postMessage("FAIL - initial document should be observable", "*");
      return;
    }
    opener.postMessage("PASS", "*");
  }
  window.addEventListener("message", e => {
    if (e.data == type) {
      verify_document_navigate_not_observable();
    }
  });
}

const setup_navigate_test = () => {
  setup_navigate_or_refresh("navigated", pre_navigate_url, post_navigate_url);
}

const setup_refresh_test = () => {
  setup_navigate_or_refresh("refreshed", pre_refresh_url, post_refresh_url);
}

const setup_back_navigation = pushed_url => {
  const verify_document_navigate_not_observable = navigated_back => {
    const entries = performance.getEntriesByType("resource");
    let found_first_document = false;
    for (entry of entries) {
      if (entry.name == pre_navigate_url) {
        found_first_document = true;
      }
      if (entry.name == post_navigate_url) {
        opener.postMessage("FAIL - navigated document exposed", "*");
        return;
      }
    }
    if (!found_first_document) {
      opener.postMessage(`FAIL - first document not exposed. navigated_back ` +
        `is ${navigated_back}`, "*");
      return;
    }
    if (navigated_back) {
      opener.postMessage("PASS", "*");
    }
  }
  window.addEventListener("message", e => {
    if (e.data == "navigated") {
      verify_document_navigate_not_observable(sessionStorage.navigated);
      if (sessionStorage.navigated) {
        delete sessionStorage.navigated;
      } else {
        sessionStorage.navigated = true;
        setTimeout(() => {
          history.pushState({}, "", pushed_url);
          location.href="navigate_back.html";
        }, 0);
      }
    }
  });
}

const open_test_window = (url, message) => {
  promise_test(() => {
    return new Promise((resolve, reject) => {
      const openee = window.open(url);
      addEventListener("message", e => {
        openee.close();
        if (e.data == "PASS") {
          resolve();
        } else {
          reject(e.data);
        }
      });
    });
  }, message);
}
