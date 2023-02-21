const load = {

  cache_bust: path => {
    let url = new URL(path, location.origin);
    url.href += (url.href.includes("?")) ? '&' : '?';
    // The `Number` type in Javascript, when interpreted as an integer, can only
    // safely represent [-2^53 + 1, 2^53 - 1] without the loss of precision [1].
    // We do not generate a global value and increment from it, as the increment
    // might not have enough precision to be reflected.
    //
    // [1]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Number
    url.href += "unique=" + Math.random().toString().substring(2);
    return url.href;
  },

  image_with_attrs: async (path, attribute_map) => {
    return new Promise(resolve => {
      const img = new Image();
      for (const key in attribute_map)
          img[key] = attribute_map[key];
      img.onload = img.onerror = resolve;
      img.src = load.cache_bust(path);
    });
  },

  // Returns a promise that settles once the given path has been fetched as an
  // image resource.
  image: path => {
    return load.image_with_attrs(path, undefined);
  },

  // Returns a promise that settles once the given path has been fetched as an
  // image resource.
  image_cors: path => load.image_with_attrs(path, {crossOrigin: "anonymous"}),

  // Returns a promise that settles once the given path has been fetched as a
  // font resource.
  font: path => {
    const div = document.createElement('div');
    div.innerHTML = `
      <style>
      @font-face {
          font-family: ahem;
          src: url('${load.cache_bust(path)}');
      }
      </style>
      <div style="font-family: ahem;">This fetches ahem font.</div>
    `;
    document.body.appendChild(div);
    return document.fonts.ready.then(() => {
      document.body.removeChild(div);
    });
  },

  stylesheet_with_attrs: async (path, attribute_map) => {
    const link = document.createElement("link");
    if (attribute_map instanceof Object) {
      for (const [key, value] of Object.entries(attribute_map)) {
        link[key] = value;
      }
    }
    link.rel = "stylesheet";
    link.type = "text/css";
    link.href = load.cache_bust(path);

    const loaded = new Promise(resolve => {
      link.onload = link.onerror = resolve;
    });

    document.head.appendChild(link);
    await loaded;
    document.head.removeChild(link);
  },

  // Returns a promise that settles once the given path has been fetched as a
  // stylesheet resource.
  stylesheet: async path => {
    return load.stylesheet_with_attrs(path, undefined);
  },

  iframe_with_attrs: async (path, attribute_map, validator, skip_wait_for_navigation) => {
    const frame = document.createElement("iframe");
    if (attribute_map instanceof Object) {
      for (const [key, value] of Object.entries(attribute_map)) {
        frame[key] = value;
      }
    }
    const loaded = new Promise(resolve => {
      frame.onload = frame.onerror = resolve;
    });
    frame.src = load.cache_bust(path);
    document.body.appendChild(frame);
    if ( !skip_wait_for_navigation ) {
      await loaded;
    }
    if (validator instanceof Function) {
      validator(frame);
    }
    // since we skipped the wait for load animation, we cannot
    // remove the iframe here since the request could get cancelled
    if ( !skip_wait_for_navigation ) {
      document.body.removeChild(frame);
    }
  },

  // Returns a promise that settles once the given path has been fetched as an
  // iframe.
  iframe: async (path, validator) => {
    return load.iframe_with_attrs(path, undefined, validator);
  },

  script_with_attrs: async (path, attribute_map) => {
    const script = document.createElement("script");
    if (attribute_map instanceof Object) {
      for (const [key, value] of Object.entries(attribute_map)) {
        script[key] = value;
      }
    }
    const loaded = new Promise(resolve => {
      script.onload = script.onerror = resolve;
    });
    script.src = load.cache_bust(path);
    document.body.appendChild(script);
    await loaded;
    document.body.removeChild(script);
  },

  // Returns a promise that settles once the given path has been fetched as a
  // script.
  script: async path => {
    return load.script_with_attrs(path, undefined);
  },

  // Returns a promise that settles once the given path has been fetched as an
  // object.
  object: async (path, type) => {
    const object = document.createElement("object");
    const object_load_settled = new Promise(resolve => {
      object.onload = object.onerror = resolve;
    });
    object.data = load.cache_bust(path);
    if (type) {
      object.type = type;
    }
    document.body.appendChild(object);
    await await_with_timeout(2000,
      "Timeout was reached before load or error events fired",
      object_load_settled,
      () => { document.body.removeChild(object) }
    );
  },

  // Returns a promise that settles once the given path has been fetched
  // through a synchronous XMLHttpRequest.
  xhr_sync: async (path, headers) => {
    const xhr = new XMLHttpRequest;
    xhr.open("GET", path, /* async = */ false);
    if (headers instanceof Object) {
      for (const [key, value] of Object.entries(headers)) {
        xhr.setRequestHeader(key, value);
      }
    }
    xhr.send();
  },

  xhr_async: path => {
    const xhr = new XMLHttpRequest();
    xhr.open("GET", path)
    xhr.send();
    return new Promise(resolve => {
      xhr.onload = xhr.onerror = resolve;
    });
  }
};
