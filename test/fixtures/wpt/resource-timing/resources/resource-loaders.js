const load = {
  _cache_bust_value: Math.random().toString().substr(2),

  cache_bust: path => {
    let url = new URL(path, location.origin);
    url.href += (url.href.includes("?")) ? '&' : '?';
    url.href += "unique=" + load._cache_bust_value++
    return url.href;
  },

  // Returns a promise that settles once the given path has been fetched as an
  // image resource.
  image: path => {
    return new Promise(resolve => {
      const img = new Image();
      img.onload = img.onerror = resolve;
      img.src = load.cache_bust(path);
    });
  },

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

  // Returns a promise that settles once the given path has been fetched as a
  // stylesheet resource.
  stylesheet: async path => {
    const link = document.createElement("link");
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

  iframe_with_attrs: async (path, attribute_map, validator) => {
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
    await loaded;
    if (validator instanceof Function) {
      validator(frame);
    }
    document.body.removeChild(frame);
  },

  // Returns a promise that settles once the given path has been fetched as an
  // iframe.
  iframe: async (path, validator) => {
    return load.iframe_with_attrs(path, undefined, validator);
  },

  // Returns a promise that settles once the given path has been fetched as a
  // script.
  script: async path => {
    const script = document.createElement("script");
    const loaded = new Promise(resolve => {
      script.onload = script.onerror = resolve;
    });
    script.src = load.cache_bust(path);
    document.body.appendChild(script);
    await loaded;
    document.body.removeChild(script);
  },

  // Returns a promise that settles once the given path has been fetched as an
  // object.
  object: async (path, type) => {
    const object = document.createElement("object");
    const loaded = new Promise(resolve => {
      object.onload = object.onerror = resolve;
    });
    object.data = load.cache_bust(path);
    if (type) {
      object.type = type;
    }
    object.style = "width: 0px; height: 0px";
    document.body.appendChild(object);
    await loaded;
    document.body.removeChild(object);
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
