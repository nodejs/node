/**
 * @fileoverview Utilities for mixed-content in web-platform-tests.
 * @author burnik@google.com (Kristijan Burnik)
 * Disclaimer: Some methods of other authors are annotated in the corresponding
 *     method's JSDoc.
 */

// ===============================================================
// Types
// ===============================================================
// Objects of the following types are used to represent what kind of
// subresource requests should be sent with what kind of policies,
// from what kind of possibly nested source contexts.
// The objects are represented as JSON objects (not JavaScript/Python classes
// in a strict sense) to be passed between JavaScript/Python code.
//
// See also common/security-features/Types.md for high-level description.

/**
  @typedef PolicyDelivery
  @type {object}
  Referrer policy etc. can be applied/delivered in several ways.
  A PolicyDelivery object specifies what policy is delivered and how.

  @property {string} deliveryType
    Specifies how the policy is delivered.
    The valid deliveryType are:

     "attr"
        [A] DOM attributes e.g. referrerPolicy.

      "rel-noref"
        [A] <link rel="noreferrer"> (referrer-policy only).

      "http-rp"
        [B] HTTP response headers.

      "meta"
        [B] <meta> elements.

  @property {string} key
  @property {string} value
    Specifies what policy to be delivered. The valid keys are:

      "referrerPolicy"
        Referrer Policy
        https://w3c.github.io/webappsec-referrer-policy/
        Valid values are those listed in
        https://w3c.github.io/webappsec-referrer-policy/#referrer-policy
        (except that "" is represented as null/None)

  A PolicyDelivery can be specified in several ways:

  - (for [A]) Associated with an individual subresource request and
    specified in `Subresource.policies`,
    e.g. referrerPolicy attributes of DOM elements.
    This is handled in invokeRequest().

  - (for [B]) Associated with an nested environmental settings object and
    specified in `SourceContext.policies`,
    e.g. HTTP referrer-policy response headers of HTML/worker scripts.
    This is handled in server-side under /common/security-features/scope/.

  - (for [B]) Associated with the top-level HTML document.
    This is handled by the generators.d
*/

/**
  @typedef Subresource
  @type {object}
  A Subresource represents how a subresource request is sent.

  @property{SubresourceType} subresourceType
    How the subresource request is sent,
    e.g. "img-tag" for sending a request via <img src>.
    See the keys of `subresourceMap` for valid values.

  @property{string} url
    subresource's URL.
    Typically this is constructed by getRequestURLs() below.

  @property{PolicyDelivery} policyDeliveries
    Policies delivered specific to the subresource request.
*/

/**
  @typedef SourceContext
  @type {object}

  @property {string} sourceContextType
    Kind of the source context to be used.
    Valid values are the keys of `sourceContextMap` below.

  @property {Array<PolicyDelivery>} policyDeliveries
    A list of PolicyDelivery applied to the source context.
*/

// ===============================================================
// General utility functions
// ===============================================================

function timeoutPromise(t, ms) {
  return new Promise(resolve => { t.step_timeout(resolve, ms); });
}

/**
 * Normalizes the target port for use in a URL. For default ports, this is the
 *     empty string (omitted port), otherwise it's a colon followed by the port
 *     number. Ports 80, 443 and an empty string are regarded as default ports.
 * @param {number} targetPort The port to use
 * @return {string} The port portion for using as part of a URL.
 */
function getNormalizedPort(targetPort) {
  return ([80, 443, ""].indexOf(targetPort) >= 0) ? "" : ":" + targetPort;
}

/**
 * Creates a GUID.
 *     See: https://en.wikipedia.org/wiki/Globally_unique_identifier
 *     Original author: broofa (http://www.broofa.com/)
 *     Sourced from: http://stackoverflow.com/a/2117523/4949715
 * @return {string} A pseudo-random GUID.
 */
function guid() {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
    var r = Math.random() * 16 | 0, v = c == 'x' ? r : (r & 0x3 | 0x8);
    return v.toString(16);
  });
}

/**
 * Initiates a new XHR via GET.
 * @param {string} url The endpoint URL for the XHR.
 * @param {string} responseType Optional - how should the response be parsed.
 *     Default is "json".
 *     See: https://xhr.spec.whatwg.org/#dom-xmlhttprequest-responsetype
 * @return {Promise} A promise wrapping the success and error events.
 */
function xhrRequest(url, responseType) {
  return new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = responseType || "json";

    xhr.addEventListener("error", function() {
      reject(Error("Network Error"));
    });

    xhr.addEventListener("load", function() {
      if (xhr.status != 200)
        reject(Error(xhr.statusText));
      else
        resolve(xhr.response);
    });

    xhr.send();
  });
}

/**
 * Sets attributes on a given DOM element.
 * @param {DOMElement} element The element on which to set the attributes.
 * @param {object} An object with keys (serving as attribute names) and values.
 */
function setAttributes(el, attrs) {
  attrs = attrs || {}
  for (var attr in attrs) {
    if (attr !== 'src')
      el.setAttribute(attr, attrs[attr]);
  }
  // Workaround for Chromium: set <img>'s src attribute after all other
  // attributes to ensure the policy is applied.
  for (var attr in attrs) {
    if (attr === 'src')
      el.setAttribute(attr, attrs[attr]);
  }
}

/**
 * Binds to success and error events of an object wrapping them into a promise
 *     available through {@code element.eventPromise}. The success event
 *     resolves and error event rejects.
 * This method adds event listeners, and then removes all the added listeners
 * when one of listened event is fired.
 * @param {object} element An object supporting events on which to bind the
 *     promise.
 * @param {string} resolveEventName [="load"] The event name to bind resolve to.
 * @param {string} rejectEventName [="error"] The event name to bind reject to.
 */
function bindEvents(element, resolveEventName, rejectEventName) {
  element.eventPromise =
      bindEvents2(element, resolveEventName, element, rejectEventName);
}

// Returns a promise wrapping success and error events of objects.
// This is a variant of bindEvents that can accept separate objects for each
// events and two events to reject, and doesn't set `eventPromise`.
//
// When `resolveObject`'s `resolveEventName` event (default: "load") is
// fired, the promise is resolved with the event.
//
// When `rejectObject`'s `rejectEventName` event (default: "error") or
// `rejectObject2`'s `rejectEventName2` event (default: "error") is
// fired, the promise is rejected.
//
// `rejectObject2` is optional.
function bindEvents2(resolveObject, resolveEventName, rejectObject, rejectEventName, rejectObject2, rejectEventName2) {
  return new Promise(function(resolve, reject) {
    const actualResolveEventName = resolveEventName || "load";
    const actualRejectEventName = rejectEventName || "error";
    const actualRejectEventName2 = rejectEventName2 || "error";

    const resolveHandler = function(event) {
      cleanup();
      resolve(event);
    };

    const rejectHandler = function(event) {
      // Chromium starts propagating errors from worker.onerror to
      // window.onerror. This handles the uncaught exceptions in tests.
      event.preventDefault();
      cleanup();
      reject(event);
    };

    const cleanup = function() {
      resolveObject.removeEventListener(actualResolveEventName, resolveHandler);
      rejectObject.removeEventListener(actualRejectEventName, rejectHandler);
      if (rejectObject2) {
        rejectObject2.removeEventListener(actualRejectEventName2, rejectHandler);
      }
    };

    resolveObject.addEventListener(actualResolveEventName, resolveHandler);
    rejectObject.addEventListener(actualRejectEventName, rejectHandler);
    if (rejectObject2) {
      rejectObject2.addEventListener(actualRejectEventName2, rejectHandler);
    }
  });
}

/**
 * Creates a new DOM element.
 * @param {string} tagName The type of the DOM element.
 * @param {object} attrs A JSON with attributes to apply to the element.
 * @param {DOMElement} parent Optional - an existing DOM element to append to
 *     If not provided, the returned element will remain orphaned.
 * @param {boolean} doBindEvents Optional - Whether to bind to load and error
 *     events and provide the promise wrapping the events via the element's
 *     {@code eventPromise} property. Default value evaluates to false.
 * @return {DOMElement} The newly created DOM element.
 */
function createElement(tagName, attrs, parentNode, doBindEvents) {
  var element = document.createElement(tagName);

  if (doBindEvents) {
    bindEvents(element);
    if (element.tagName == "IFRAME" && !('srcdoc' in attrs || 'src' in attrs)) {
      // If we're loading a frame, ensure we spin the event loop after load to
      // paper over the different event timing in Gecko vs Blink/WebKit
      // see https://github.com/whatwg/html/issues/4965
      element.eventPromise = element.eventPromise.then(() => {
        return new Promise(resolve => setTimeout(resolve, 0))
      });
    }
  }
  // We set the attributes after binding to events to catch any
  // event-triggering attribute changes. E.g. form submission.
  //
  // But be careful with images: unlike other elements they will start the load
  // as soon as the attr is set, even if not in the document yet, and sometimes
  // complete it synchronously, so the append doesn't have the effect we want.
  // So for images, we want to set the attrs after appending, whereas for other
  // elements we want to do it before appending.
  var isImg = (tagName == "img");
  if (!isImg)
    setAttributes(element, attrs);

  if (parentNode)
    parentNode.appendChild(element);

  if (isImg)
    setAttributes(element, attrs);

  return element;
}

function createRequestViaElement(tagName, attrs, parentNode) {
  return createElement(tagName, attrs, parentNode, true).eventPromise;
}

function wrapResult(server_data) {
  if (typeof(server_data) === "string") {
    throw server_data;
  }
  return {
    referrer: server_data.headers.referer,
    headers: server_data.headers
  }
}

// ===============================================================
// Subresources
// ===============================================================

/**
  @typedef RequestResult
  @type {object}
  Represents the result of sending an request.
  All properties are optional. See the comments for
  requestVia*() and invokeRequest() below to see which properties are set.

  @property {Array<Object<string, string>>} headers
    HTTP request headers sent to server.
  @property {string} referrer - Referrer.
  @property {string} location - The URL of the subresource.
  @property {string} sourceContextUrl
    the URL of the global object where the actual request is sent.
*/

/**
  requestVia*(url, additionalAttributes) functions send a subresource
  request from the current environment settings object.

  @param {string} url
    The URL of the subresource.
  @param {Object<string, string>} additionalAttributes
    Additional attributes set to DOM elements
    (element-initiated requests only).

  @returns {Promise} that are resolved with a RequestResult object
  on successful requests.

  - Category 1:
      `headers`: set.
      `referrer`: set via `document.referrer`.
      `location`: set via `document.location`.
      See `template/document.html.template`.
  - Category 2:
      `headers`: set.
      `referrer`: set to `headers.referer` by `wrapResult()`.
      `location`: not set.
  - Category 3:
      All the keys listed above are NOT set.
  `sourceContextUrl` is not set here.

  -------------------------------- -------- --------------------------
  Function name                    Category Used in
                                            -------- ------- ---------
                                            referrer mixed-  upgrade-
                                            policy   content insecure-
                                            policy   content request
  -------------------------------- -------- -------- ------- ---------
  requestViaAnchor                 1        Y        Y       -
  requestViaArea                   1        Y        Y       -
  requestViaAudio                  3        -        Y       -
  requestViaDedicatedWorker        2        Y        Y       Y
  requestViaFetch                  2        Y        Y       -
  requestViaForm                   2        -        Y       -
  requestViaIframe                 1        Y        Y       -
  requestViaImage                  2        Y        Y       -
  requestViaLinkPrefetch           3        -        Y       -
  requestViaLinkStylesheet         3        -        Y       -
  requestViaObject                 3        -        Y       -
  requestViaPicture                3        -        Y       -
  requestViaScript                 2        Y        Y       -
  requestViaSendBeacon             3        -        Y       -
  requestViaSharedWorker           2        Y        Y       Y
  requestViaVideo                  3        -        Y       -
  requestViaWebSocket              3        -        Y       -
  requestViaWorklet                3        -        Y       Y
  requestViaXhr                    2        Y        Y       -
  -------------------------------- -------- -------- ------- ---------
*/

/**
 * Creates a new iframe, binds load and error events, sets the src attribute and
 *     appends it to {@code document.body} .
 * @param {string} url The src for the iframe.
 * @return {Promise} The promise for success/error events.
 */
function requestViaIframe(url, additionalAttributes) {
  const iframe = createElement(
      "iframe",
      Object.assign({"src": url}, additionalAttributes),
      document.body,
      false);
  return bindEvents2(window, "message", iframe, "error", window, "error")
      .then(event => {
          if (event.source !== iframe.contentWindow)
            return Promise.reject(new Error('Unexpected event.source'));
          return event.data;
        });
}

/**
 * Creates a new image, binds load and error events, sets the src attribute and
 *     appends it to {@code document.body} .
 * @param {string} url The src for the image.
 * @return {Promise} The promise for success/error events.
 */
function requestViaImage(url, additionalAttributes) {
  const img = createElement(
      "img",
      // crossOrigin attribute is added to read the pixel data of the response.
      Object.assign({"src": url, "crossOrigin": "Anonymous"}, additionalAttributes),
      document.body, true);
  return img.eventPromise.then(() => wrapResult(decodeImageData(img)));
}

// Helper for requestViaImage().
function decodeImageData(img) {
  var canvas = document.createElement("canvas");
  var context = canvas.getContext('2d');
  context.drawImage(img, 0, 0);
  var imgData = context.getImageData(0, 0, img.clientWidth, img.clientHeight);
  const rgba = imgData.data;

  let decodedBytes = new Uint8ClampedArray(rgba.length);
  let decodedLength = 0;

  for (var i = 0; i + 12 <= rgba.length; i += 12) {
    // A single byte is encoded in three pixels. 8 pixel octets (among
    // 9 octets = 3 pixels * 3 channels) are used to encode 8 bits,
    // the most significant bit first, where `0` and `255` in pixel values
    // represent `0` and `1` in bits, respectively.
    // This encoding is used to avoid errors due to different color spaces.
    const bits = [];
    for (let j = 0; j < 3; ++j) {
      bits.push(rgba[i + j * 4 + 0]);
      bits.push(rgba[i + j * 4 + 1]);
      bits.push(rgba[i + j * 4 + 2]);
      // rgba[i + j * 4 + 3]: Skip alpha channel.
    }
    // The last one element is not used.
    bits.pop();

    // Decode a single byte.
    let byte = 0;
    for (let j = 0; j < 8; ++j) {
      byte <<= 1;
      if (bits[j] >= 128)
        byte |= 1;
    }

    // Zero is the string terminator.
    if (byte == 0)
      break;

    decodedBytes[decodedLength++] = byte;
  }

  // Remove trailing nulls from data.
  decodedBytes = decodedBytes.subarray(0, decodedLength);
  var string_data = (new TextDecoder("ascii")).decode(decodedBytes);

  return JSON.parse(string_data);
}

/**
 * Initiates a new XHR GET request to provided URL.
 * @param {string} url The endpoint URL for the XHR.
 * @return {Promise} The promise for success/error events.
 */
function requestViaXhr(url) {
  return xhrRequest(url).then(result => wrapResult(result));
}

/**
 * Initiates a new GET request to provided URL via the Fetch API.
 * @param {string} url The endpoint URL for the Fetch.
 * @return {Promise} The promise for success/error events.
 */
function requestViaFetch(url) {
  return fetch(url)
    .then(res => res.json())
    .then(j => wrapResult(j));
}

function dedicatedWorkerUrlThatFetches(url) {
  return `data:text/javascript,
    fetch('${url}')
      .then(r => r.json())
      .then(j => postMessage(j))
      .catch((e) => postMessage(e.message));`;
}

function workerUrlThatImports(url, additionalAttributes) {
  let csp = "";
  if (additionalAttributes && additionalAttributes.contentSecurityPolicy) {
    csp=`&contentSecurityPolicy=${additionalAttributes.contentSecurityPolicy}`;
  }
  return `/common/security-features/subresource/static-import.py` +
      `?import_url=${encodeURIComponent(url)}${csp}`;
}

function workerDataUrlThatImports(url) {
  return `data:text/javascript,import '${url}';`;
}

/**
 * Creates a new Worker, binds message and error events wrapping them into.
 *     {@code worker.eventPromise} and posts an empty string message to start
 *     the worker.
 * @param {string} url The endpoint URL for the worker script.
 * @param {object} options The options for Worker constructor.
 * @return {Promise} The promise for success/error events.
 */
function requestViaDedicatedWorker(url, options) {
  var worker;
  try {
    worker = new Worker(url, options);
  } catch (e) {
    return Promise.reject(e);
  }
  worker.postMessage('');
  return bindEvents2(worker, "message", worker, "error")
    .then(event => wrapResult(event.data));
}

function requestViaSharedWorker(url, options) {
  var worker;
  try {
    worker = new SharedWorker(url, options);
  } catch(e) {
    return Promise.reject(e);
  }
  const promise = bindEvents2(worker.port, "message", worker, "error")
    .then(event => wrapResult(event.data));
  worker.port.start();
  return promise;
}

// Returns a reference to a worklet object corresponding to a given type.
function get_worklet(type) {
  if (type == 'animation')
    return CSS.animationWorklet;
  if (type == 'layout')
    return CSS.layoutWorklet;
  if (type == 'paint')
    return CSS.paintWorklet;
  if (type == 'audio')
    return new OfflineAudioContext(2,44100*40,44100).audioWorklet;

  throw new Error('unknown worklet type is passed.');
}

function requestViaWorklet(type, url) {
  try {
    return get_worklet(type).addModule(url);
  } catch (e) {
    return Promise.reject(e);
  }
}

/**
 * Creates a navigable element with the name `navigableElementName`
 * (<a>, <area>, or <form>) under `parentNode`, and
 * performs a navigation by `trigger()` (e.g. clicking <a>).
 * To avoid navigating away from the current execution context,
 * a target attribute is set to point to a new helper iframe.
 * @param {string} navigableElementName
 * @param {object} additionalAttributes The attributes of the navigable element.
 * @param {DOMElement} parentNode
 * @param {function(DOMElement} trigger A callback called after the navigable
 * element is inserted and should trigger navigation using the element.
 * @return {Promise} The promise for success/error events.
 */
function requestViaNavigable(navigableElementName, additionalAttributes,
                             parentNode, trigger) {
  const name = guid();

  const iframe =
    createElement("iframe", {"name": name, "id": name}, parentNode, false);

  const navigable = createElement(
      navigableElementName,
      Object.assign({"target": name}, additionalAttributes),
      parentNode, false);

  const promise =
    bindEvents2(window, "message", iframe, "error", window, "error")
      .then(event => {
          if (event.source !== iframe.contentWindow)
            return Promise.reject(new Error('Unexpected event.source'));
          return event.data;
        });
  trigger(navigable);
  return promise;
}

/**
 * Creates a new anchor element, appends it to {@code document.body} and
 *     performs the navigation.
 * @param {string} url The URL to navigate to.
 * @return {Promise} The promise for success/error events.
 */
function requestViaAnchor(url, additionalAttributes) {
  return requestViaNavigable(
      "a",
      Object.assign({"href": url, "innerHTML": "Link to resource"},
                    additionalAttributes),
      document.body, a => a.click());
}

/**
 * Creates a new area element, appends it to {@code document.body} and performs
 *     the navigation.
 * @param {string} url The URL to navigate to.
 * @return {Promise} The promise for success/error events.
 */
function requestViaArea(url, additionalAttributes) {
  // TODO(kristijanburnik): Append to map and add image.
  return requestViaNavigable(
      "area",
      Object.assign({"href": url}, additionalAttributes),
      document.body, area => area.click());
}

/**
 * Creates a new script element, sets the src to url, and appends it to
 *     {@code document.body}.
 * @param {string} url The src URL.
 * @return {Promise} The promise for success/error events.
 */
function requestViaScript(url, additionalAttributes) {
  const script = createElement(
      "script",
      Object.assign({"src": url}, additionalAttributes),
      document.body,
      false);

  return bindEvents2(window, "message", script, "error", window, "error")
    .then(event => wrapResult(event.data));
}

/**
 * Creates a new script element that performs a dynamic import to `url`, and
 * appends the script element to {@code document.body}.
 * @param {string} url The src URL.
 * @return {Promise} The promise for success/error events.
 */
function requestViaDynamicImport(url, additionalAttributes) {
  const scriptUrl = `data:text/javascript,import("${url}");`;
  const script = createElement(
      "script",
      Object.assign({"src": scriptUrl}, additionalAttributes),
      document.body,
      false);

  return bindEvents2(window, "message", script, "error", window, "error")
    .then(event => wrapResult(event.data));
}

/**
 * Creates a new form element, sets attributes, appends it to
 *     {@code document.body} and submits the form.
 * @param {string} url The URL to submit to.
 * @return {Promise} The promise for success/error events.
 */
function requestViaForm(url, additionalAttributes) {
  return requestViaNavigable(
      "form",
      Object.assign({"action": url, "method": "POST"}, additionalAttributes),
      document.body, form => form.submit());
}

/**
 * Creates a new link element for a stylesheet, binds load and error events,
 *     sets the href to url and appends it to {@code document.head}.
 * @param {string} url The URL for a stylesheet.
 * @return {Promise} The promise for success/error events.
 */
function requestViaLinkStylesheet(url) {
  return createRequestViaElement("link",
                                 {"rel": "stylesheet", "href": url},
                                 document.head);
}

/**
 * Creates a new link element for a prefetch, binds load and error events, sets
 *     the href to url and appends it to {@code document.head}.
 * @param {string} url The URL of a resource to prefetch.
 * @return {Promise} The promise for success/error events.
 */
function requestViaLinkPrefetch(url) {
  var link = document.createElement('link');
  if (link.relList && link.relList.supports && link.relList.supports("prefetch")) {
    return createRequestViaElement("link",
                                   {"rel": "prefetch", "href": url},
                                   document.head);
  } else {
    return Promise.reject("This browser does not support 'prefetch'.");
  }
}

/**
 * Initiates a new beacon request.
 * @param {string} url The URL of a resource to prefetch.
 * @return {Promise} The promise for success/error events.
 */
async function requestViaSendBeacon(url) {
  function wait(ms) {
    return new Promise(resolve => step_timeout(resolve, ms));
  }
  if (!navigator.sendBeacon(url)) {
    // If mixed-content check fails, it should return false.
    throw new Error('sendBeacon() fails.');
  }
  // We don't have a means to see the result of sendBeacon() request
  // for sure. Let's wait for a while and let the generic test function
  // ask the server for the result.
  await wait(500);
  return 'allowed';
}

/**
 * Creates a new media element with a child source element, binds loadeddata and
 *     error events, sets attributes and appends to document.body.
 * @param {string} type The type of the media element (audio/video/picture).
 * @param {object} media_attrs The attributes for the media element.
 * @param {object} source_attrs The attributes for the child source element.
 * @return {DOMElement} The newly created media element.
 */
function createMediaElement(type, media_attrs, source_attrs) {
  var mediaElement = createElement(type, {});

  var sourceElement = createElement("source", {});

  mediaElement.eventPromise = new Promise(function(resolve, reject) {
    mediaElement.addEventListener("loadeddata", function (e) {
      resolve(e);
    });

    // Safari doesn't fire an `error` event when blocking mixed content.
    mediaElement.addEventListener("stalled", function(e) {
      reject(e);
    });

    sourceElement.addEventListener("error", function(e) {
      reject(e);
    });
  });

  setAttributes(mediaElement, media_attrs);
  setAttributes(sourceElement, source_attrs);

  mediaElement.appendChild(sourceElement);
  document.body.appendChild(mediaElement);

  return mediaElement;
}

/**
 * Creates a new video element, binds loadeddata and error events, sets
 *     attributes and source URL and appends to {@code document.body}.
 * @param {string} url The URL of the video.
 * @return {Promise} The promise for success/error events.
 */
function requestViaVideo(url) {
  return createMediaElement("video",
                            {},
                            {"src": url}).eventPromise;
}

/**
 * Creates a new audio element, binds loadeddata and error events, sets
 *     attributes and source URL and appends to {@code document.body}.
 * @param {string} url The URL of the audio.
 * @return {Promise} The promise for success/error events.
 */
function requestViaAudio(url) {
  return createMediaElement("audio",
                            {},
                            {"type": "audio/wav", "src": url}).eventPromise;
}

/**
 * Creates a new picture element, binds loadeddata and error events, sets
 *     attributes and source URL and appends to {@code document.body}. Also
 *     creates new image element appending it to the picture
 * @param {string} url The URL of the image for the source and image elements.
 * @return {Promise} The promise for success/error events.
 */
function requestViaPicture(url) {
  var picture = createMediaElement("picture", {}, {"srcset": url,
                                                "type": "image/png"});
  return createRequestViaElement("img", {"src": url}, picture);
}

/**
 * Creates a new object element, binds load and error events, sets the data to
 *     url, and appends it to {@code document.body}.
 * @param {string} url The data URL.
 * @return {Promise} The promise for success/error events.
 */
function requestViaObject(url) {
  return createRequestViaElement("object", {"data": url, "type": "text/html"}, document.body);
}

/**
 * Creates a new WebSocket pointing to {@code url} and sends a message string
 * "echo". The {@code message} and {@code error} events are triggering the
 * returned promise resolve/reject events.
 * @param {string} url The URL for WebSocket to connect to.
 * @return {Promise} The promise for success/error events.
 */
function requestViaWebSocket(url) {
  return new Promise(function(resolve, reject) {
    var websocket = new WebSocket(url);

    websocket.addEventListener("message", function(e) {
      resolve(e.data);
    });

    websocket.addEventListener("open", function(e) {
      websocket.send("echo");
    });

    websocket.addEventListener("error", function(e) {
      reject(e)
    });
  })
  .then(data => {
      return JSON.parse(data);
    });
}

/**
  @typedef SubresourceType
  @type {string}

  Represents how a subresource is sent.
  The keys of `subresourceMap` below are the valid values.
*/

// Subresource paths and invokers.
const subresourceMap = {
  "a-tag": {
    path: "/common/security-features/subresource/document.py",
    invoker: requestViaAnchor,
  },
  "area-tag": {
    path: "/common/security-features/subresource/document.py",
    invoker: requestViaArea,
  },
  "audio-tag": {
    path: "/common/security-features/subresource/audio.py",
    invoker: requestViaAudio,
  },
  "beacon": {
    path: "/common/security-features/subresource/empty.py",
    invoker: requestViaSendBeacon,
  },
  "fetch": {
    path: "/common/security-features/subresource/xhr.py",
    invoker: requestViaFetch,
  },
  "form-tag": {
    path: "/common/security-features/subresource/document.py",
    invoker: requestViaForm,
  },
  "iframe-tag": {
    path: "/common/security-features/subresource/document.py",
    invoker: requestViaIframe,
  },
  "img-tag": {
    path: "/common/security-features/subresource/image.py",
    invoker: requestViaImage,
  },
  "link-css-tag": {
    path: "/common/security-features/subresource/empty.py",
    invoker: requestViaLinkStylesheet,
  },
  "link-prefetch-tag": {
    path: "/common/security-features/subresource/empty.py",
    invoker: requestViaLinkPrefetch,
  },
  "object-tag": {
    path: "/common/security-features/subresource/empty.py",
    invoker: requestViaObject,
  },
  "picture-tag": {
    path: "/common/security-features/subresource/image.py",
    invoker: requestViaPicture,
  },
  "script-tag": {
    path: "/common/security-features/subresource/script.py",
    invoker: requestViaScript,
  },
  "script-tag-dynamic-import": {
    path: "/common/security-features/subresource/script.py",
    invoker: requestViaDynamicImport,
  },
  "video-tag": {
    path: "/common/security-features/subresource/video.py",
    invoker: requestViaVideo,
  },
  "xhr": {
    path: "/common/security-features/subresource/xhr.py",
    invoker: requestViaXhr,
  },

  "worker-classic": {
    path: "/common/security-features/subresource/worker.py",
    invoker: url => requestViaDedicatedWorker(url),
  },
  "worker-module": {
    path: "/common/security-features/subresource/worker.py",
    invoker: url => requestViaDedicatedWorker(url, {type: "module"}),
  },
  "worker-import": {
    path: "/common/security-features/subresource/worker.py",
    invoker: (url, additionalAttributes) =>
        requestViaDedicatedWorker(workerUrlThatImports(url, additionalAttributes), {type: "module"}),
  },
  "worker-import-data": {
    path: "/common/security-features/subresource/worker.py",
    invoker: url =>
        requestViaDedicatedWorker(workerDataUrlThatImports(url), {type: "module"}),
  },
  "sharedworker-classic": {
    path: "/common/security-features/subresource/shared-worker.py",
    invoker: url => requestViaSharedWorker(url),
  },
  "sharedworker-module": {
    path: "/common/security-features/subresource/shared-worker.py",
    invoker: url => requestViaSharedWorker(url, {type: "module"}),
  },
  "sharedworker-import": {
    path: "/common/security-features/subresource/shared-worker.py",
    invoker: (url, additionalAttributes) =>
        requestViaSharedWorker(workerUrlThatImports(url, additionalAttributes), {type: "module"}),
  },
  "sharedworker-import-data": {
    path: "/common/security-features/subresource/shared-worker.py",
    invoker: url =>
        requestViaSharedWorker(workerDataUrlThatImports(url), {type: "module"}),
  },

  "websocket": {
    path: "/stash_responder",
    invoker: requestViaWebSocket,
  },
};
for (const workletType of ['animation', 'audio', 'layout', 'paint']) {
  subresourceMap[`worklet-${workletType}`] = {
      path: "/common/security-features/subresource/worker.py",
      invoker: url => requestViaWorklet(workletType, url)
    };
  subresourceMap[`worklet-${workletType}-import-data`] = {
      path: "/common/security-features/subresource/worker.py",
      invoker: url =>
          requestViaWorklet(workletType, workerDataUrlThatImports(url))
    };
}

/**
  @typedef RedirectionType
  @type {string}

  Represents what redirects should occur to the subresource request
  after initial request.
  See preprocess_redirection() in
  /common/security-features/subresource/subresource.py for valid values.
*/

/**
  Construct subresource (and related) origin.

  @param {string} originType
  @returns {object} the origin of the subresource.
*/
function getSubresourceOrigin(originType) {
  const httpProtocol = "http";
  const httpsProtocol = "https";
  const wsProtocol = "ws";
  const wssProtocol = "wss";

  const sameOriginHost = "{{host}}";
  const crossOriginHost = "{{domains[www1]}}";

  // These values can evaluate to either empty strings or a ":port" string.
  const httpPort = getNormalizedPort(parseInt("{{ports[http][0]}}", 10));
  const httpsRawPort = parseInt("{{ports[https][0]}}", 10);
  const httpsPort = getNormalizedPort(httpsRawPort);
  const wsPort = getNormalizedPort(parseInt("{{ports[ws][0]}}", 10));
  const wssRawPort = parseInt("{{ports[wss][0]}}", 10);
  const wssPort = getNormalizedPort(wssRawPort);

  /**
    @typedef OriginType
    @type {string}

    Represents the origin of the subresource request URL.
    The keys of `originMap` below are the valid values.

    Note that there can be redirects from the specified origin
    (see RedirectionType), and thus the origin of the subresource
    response URL might be different from what is specified by OriginType.
  */
  const originMap = {
    "same-https": httpsProtocol + "://" + sameOriginHost + httpsPort,
    "same-http": httpProtocol + "://" + sameOriginHost + httpPort,
    "cross-https": httpsProtocol + "://" + crossOriginHost + httpsPort,
    "cross-http": httpProtocol + "://" + crossOriginHost + httpPort,
    "same-wss": wssProtocol + "://" + sameOriginHost + wssPort,
    "same-ws": wsProtocol + "://" + sameOriginHost + wsPort,
    "cross-wss": wssProtocol + "://" + crossOriginHost + wssPort,
    "cross-ws": wsProtocol + "://" + crossOriginHost + wsPort,

    // The following origin types are used for upgrade-insecure-requests tests:
    // These rely on some unintuitive cleverness due to WPT's test setup:
    // 'Upgrade-Insecure-Requests' does not upgrade the port number,
    // so we use URLs in the form `http://[domain]:[https-port]`,
    // which will be upgraded to `https://[domain]:[https-port]`.
    // If the upgrade fails, the load will fail, as we don't serve HTTP over
    // the secure port.
    "same-http-downgrade":
        httpProtocol + "://" + sameOriginHost + ":" + httpsRawPort,
    "cross-http-downgrade":
        httpProtocol + "://" + crossOriginHost + ":" + httpsRawPort,
    "same-ws-downgrade":
        wsProtocol + "://" + sameOriginHost + ":" + wssRawPort,
    "cross-ws-downgrade":
        wsProtocol + "://" + crossOriginHost + ":" + wssRawPort,
  };

  return originMap[originType];
}

/**
  Construct subresource (and related) URLs.

  @param {SubresourceType} subresourceType
  @param {OriginType} originType
  @param {RedirectionType} redirectionType
  @returns {object} with following properties:
    {string} testUrl
      The subresource request URL.
    {string} announceUrl
    {string} assertUrl
      The URLs to be used for detecting whether `testUrl` is actually sent
      to the server.
      1. Fetch `announceUrl` first,
      2. then possibly fetch `testUrl`, and
      3. finally fetch `assertUrl`.
         The fetch result of `assertUrl` should indicate whether
         `testUrl` is actually sent to the server or not.
*/
function getRequestURLs(subresourceType, originType, redirectionType) {
  const key = guid();
  const value = guid();

  // We use the same stash path for both HTTP/S and WS/S stash requests.
  const stashPath = encodeURIComponent("/mixed-content");

  const stashEndpoint = "/common/security-features/subresource/xhr.py?key=" +
                        key + "&path=" + stashPath;
  return {
    testUrl:
      getSubresourceOrigin(originType) +
        subresourceMap[subresourceType].path +
        "?redirection=" + encodeURIComponent(redirectionType) +
        "&action=purge&key=" + key +
        "&path=" + stashPath,
    announceUrl: stashEndpoint + "&action=put&value=" + value,
    assertUrl: stashEndpoint + "&action=take",
  };
}

// ===============================================================
// Source Context
// ===============================================================
// Requests can be sent from several source contexts,
// such as the main documents, iframes, workers, or so,
// possibly nested, and possibly with <meta>/http headers added.
// invokeRequest() and invokeFrom*() functions handles
// SourceContext-related setup in client-side.

/**
  invokeRequest() invokes a subresource request
  (specified as `subresource`)
  from a (possibly nested) environment settings object
  (specified as `sourceContextList`).

  For nested contexts, invokeRequest() calls an invokeFrom*() function
  that creates a nested environment settings object using
  /common/security-features/scope/, which calls invokeRequest()
  again inside the nested environment settings object.
  This cycle continues until all specified
  nested environment settings object are created, and
  finally invokeRequest() calls a requestVia*() function to start the
  subresource request from the inner-most environment settings object.

  @param {Subresource} subresource
  @param {Array<SourceContext>} sourceContextList

  @returns {Promise} A promise that is resolved with an RequestResult object.
  `sourceContextUrl` is always set. For whether other properties are set,
  see the comments for requestVia*() above.
*/
function invokeRequest(subresource, sourceContextList) {
  if (sourceContextList.length === 0) {
    // No further nested global objects. Send the subresource request here.

    const additionalAttributes = {};
    /** @type {PolicyDelivery} policyDelivery */
    for (const policyDelivery of (subresource.policyDeliveries || [])) {
      // Depending on the delivery method, extend the subresource element with
      // these attributes.
      if (policyDelivery.deliveryType === "attr") {
        additionalAttributes[policyDelivery.key] = policyDelivery.value;
      } else if (policyDelivery.deliveryType === "rel-noref") {
        additionalAttributes["rel"] = "noreferrer";
      } else if (policyDelivery.deliveryType === "http-rp") {
        additionalAttributes[policyDelivery.key] = policyDelivery.value;
      } else if (policyDelivery.deliveryType === "meta") {
        additionalAttributes[policyDelivery.key] = policyDelivery.value;
      }
    }

    return subresourceMap[subresource.subresourceType].invoker(
        subresource.url,
        additionalAttributes)
      .then(result => Object.assign(
          {sourceContextUrl: location.toString()},
          result));
  }

  // Defines invokers for each valid SourceContext.sourceContextType.
  const sourceContextMap = {
    "srcdoc": { // <iframe srcdoc></iframe>
      invoker: invokeFromIframe,
    },
    "iframe": { // <iframe src="same-origin-URL"></iframe>
      invoker: invokeFromIframe,
    },
    "iframe-blank": { // <iframe></iframe>
      invoker: invokeFromIframe,
    },
    "worker-classic": {
      // Classic dedicated worker loaded from same-origin.
      invoker: invokeFromWorker.bind(undefined, "worker", false, {}),
    },
    "worker-classic-data": {
      // Classic dedicated worker loaded from data: URL.
      invoker: invokeFromWorker.bind(undefined, "worker", true, {}),
    },
    "worker-module": {
      // Module dedicated worker loaded from same-origin.
      invoker: invokeFromWorker.bind(undefined, "worker", false, {type: 'module'}),
    },
    "worker-module-data": {
      // Module dedicated worker loaded from data: URL.
      invoker: invokeFromWorker.bind(undefined, "worker", true, {type: 'module'}),
    },
    "sharedworker-classic": {
      // Classic shared worker loaded from same-origin.
      invoker: invokeFromWorker.bind(undefined, "sharedworker", false, {}),
    },
    "sharedworker-classic-data": {
      // Classic shared worker loaded from data: URL.
      invoker: invokeFromWorker.bind(undefined, "sharedworker", true, {}),
    },
    "sharedworker-module": {
      // Module shared worker loaded from same-origin.
      invoker: invokeFromWorker.bind(undefined, "sharedworker", false, {type: 'module'}),
    },
    "sharedworker-module-data": {
      // Module shared worker loaded from data: URL.
      invoker: invokeFromWorker.bind(undefined, "sharedworker", true, {type: 'module'}),
    },
  };

  return sourceContextMap[sourceContextList[0].sourceContextType].invoker(
      subresource, sourceContextList);
}

// Quick hack to expose invokeRequest when common.sub.js is loaded either
// as a classic or module script.
self.invokeRequest = invokeRequest;

/**
  invokeFrom*() functions are helper functions with the same parameters
  and return values as invokeRequest(), that are tied to specific types
  of top-most environment settings objects.
  For example, invokeFromIframe() is the helper function for the cases where
  sourceContextList[0] is an iframe.
*/

/**
  @param {string} workerType
    "worker" (for dedicated worker) or "sharedworker".
  @param {boolean} isDataUrl
    true if the worker script is loaded from data: URL.
    Otherwise, the script is loaded from same-origin.
  @param {object} workerOptions
    The `options` argument for Worker constructor.

  Other parameters and return values are the same as those of invokeRequest().
*/
function invokeFromWorker(workerType, isDataUrl, workerOptions,
                          subresource, sourceContextList) {
  const currentSourceContext = sourceContextList[0];
  let workerUrl =
    "/common/security-features/scope/worker.py?policyDeliveries=" +
    encodeURIComponent(JSON.stringify(
        currentSourceContext.policyDeliveries || []));
  if (workerOptions.type === 'module') {
    workerUrl += "&type=module";
  }

  let promise;
  if (isDataUrl) {
    promise = fetch(workerUrl)
      .then(r => r.text())
      .then(source => {
          return 'data:text/javascript;base64,' + btoa(source);
        });
  } else {
    promise = Promise.resolve(workerUrl);
  }

  return promise
    .then(url => {
      if (workerType === "worker") {
        const worker = new Worker(url, workerOptions);
        worker.postMessage({subresource: subresource,
                            sourceContextList: sourceContextList.slice(1)});
        return bindEvents2(worker, "message", worker, "error", window, "error");
      } else if (workerType === "sharedworker") {
        const worker = new SharedWorker(url, workerOptions);
        worker.port.start();
        worker.port.postMessage({subresource: subresource,
                                 sourceContextList: sourceContextList.slice(1)});
        return bindEvents2(worker.port, "message", worker, "error", window, "error");
      } else {
        throw new Error('Invalid worker type: ' + workerType);
      }
    })
    .then(event => {
        if (event.data.error)
          return Promise.reject(event.data.error);
        return event.data;
      });
}

function invokeFromIframe(subresource, sourceContextList) {
  const currentSourceContext = sourceContextList[0];
  const frameUrl =
    "/common/security-features/scope/document.py?policyDeliveries=" +
    encodeURIComponent(JSON.stringify(
        currentSourceContext.policyDeliveries || []));

  let iframe;
  let promise;
  if (currentSourceContext.sourceContextType === 'srcdoc') {
    promise = fetch(frameUrl)
      .then(r => r.text())
      .then(srcdoc => {
          iframe = createElement(
              "iframe", {srcdoc: srcdoc}, document.body, true);
          return iframe.eventPromise;
        });
  } else if (currentSourceContext.sourceContextType === 'iframe') {
    iframe = createElement("iframe", {src: frameUrl}, document.body, true);
    promise = iframe.eventPromise;
  } else if (currentSourceContext.sourceContextType === 'iframe-blank') {
    let frameContent;
    promise = fetch(frameUrl)
      .then(r => r.text())
      .then(t => {
          frameContent = t;
          iframe = createElement("iframe", {}, document.body, true);
          return iframe.eventPromise;
        })
      .then(() => {
          // Reinitialize `iframe.eventPromise` with a new promise
          // that catches the load event for the document.write() below.
          bindEvents(iframe);

          iframe.contentDocument.write(frameContent);
          iframe.contentDocument.close();
          return iframe.eventPromise;
        });
  }

  return promise
    .then(() => {
        const promise = bindEvents2(
            window, "message", iframe, "error", window, "error");
        iframe.contentWindow.postMessage(
            {subresource: subresource,
             sourceContextList: sourceContextList.slice(1)},
            "*");
        return promise;
      })
    .then(event => {
        if (event.data.error)
          return Promise.reject(event.data.error);
        return event.data;
      });
}

// SanityChecker does nothing in release mode. See sanity-checker.js for debug
// mode.
function SanityChecker() {}
SanityChecker.prototype.checkScenario = function() {};
SanityChecker.prototype.setFailTimeout = function(test, timeout) {};
SanityChecker.prototype.checkSubresourceResult = function() {};
