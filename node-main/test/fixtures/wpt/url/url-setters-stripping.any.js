function urlString({ scheme = "https",
                     username = "username",
                     password = "password",
                     host = "host",
                     port = "8000",
                     pathname = "path",
                     search = "query",
                     hash = "fragment" }) {
  return `${scheme}://${username}:${password}@${host}:${port}/${pathname}?${search}#${hash}`;
}

function urlRecord(scheme) {
  return new URL(urlString({ scheme }));
}

for(const scheme of ["https", "wpt++"]) {
  for(let i = 0; i < 0x20; i++) {
    const stripped = i === 0x09 || i === 0x0A || i === 0x0D;

    // It turns out that user agents are surprisingly similar for these ranges so generate fewer
    // tests. If this is changed also change the logic for host below.
    if (i !== 0 && i !== 0x1F && !stripped) {
      continue;
    }

    const cpString = String.fromCodePoint(i);
    const cpReference = "U+" + i.toString(16).toUpperCase().padStart(4, "0");

    test(() => {
      const expected = scheme === "https" ? (stripped ? "http" : "https") : (stripped ? "wpt--" : "wpt++");
      const url = urlRecord(scheme);
      url.protocol = String.fromCodePoint(i) + (scheme === "https" ? "http" : "wpt--");
      assert_equals(url.protocol, expected + ":", "property");
      assert_equals(url.href, urlString({ scheme: expected }), "href");
    }, `Setting protocol with leading ${cpReference} (${scheme}:)`);

    test(() => {
      const expected = scheme === "https" ? (stripped ? "http" : "https") : (stripped ? "wpt--" : "wpt++");
      const url = urlRecord(scheme);
      url.protocol = (scheme === "https" ? "http" : "wpt--") + String.fromCodePoint(i);
      assert_equals(url.protocol, expected + ":", "property");
      assert_equals(url.href, urlString({ scheme: expected }), "href");
    }, `Setting protocol with ${cpReference} before inserted colon (${scheme}:)`);

    // Cannot test protocol with trailing as the algorithm inserts a colon before proceeding

    // These do no stripping
    for (const property of ["username", "password"]) {
      for (const [type, expected, input] of [
        ["leading", encodeURIComponent(cpString) + "test", String.fromCodePoint(i) + "test"],
        ["middle", "te" + encodeURIComponent(cpString) + "st", "te" + String.fromCodePoint(i) + "st"],
        ["trailing", "test" + encodeURIComponent(cpString), "test" + String.fromCodePoint(i)]
      ]) {
        test(() => {
          const url = urlRecord(scheme);
          url[property] = input;
          assert_equals(url[property], expected, "property");
          assert_equals(url.href, urlString({ scheme, [property]: expected }), "href");
        }, `Setting ${property} with ${type} ${cpReference} (${scheme}:)`);
      }
    }

    for (const [type, expectedPart, input] of [
      ["leading", (scheme === "https" ? cpString : encodeURIComponent(cpString)) + "test", String.fromCodePoint(i) + "test"],
      ["middle", "te" + (scheme === "https" ? cpString : encodeURIComponent(cpString)) + "st", "te" + String.fromCodePoint(i) + "st"],
      ["trailing", "test" + (scheme === "https" ? cpString : encodeURIComponent(cpString)), "test" + String.fromCodePoint(i)]
    ]) {
      test(() => {
        const expected = i === 0x00 || (scheme === "https" && i === 0x1F) ? "host" : stripped ? "test" : expectedPart;
        const url = urlRecord(scheme);
        url.host = input;
        assert_equals(url.host, expected + ":8000", "property");
        assert_equals(url.href, urlString({ scheme, host: expected }), "href");
      }, `Setting host with ${type} ${cpReference} (${scheme}:)`);

      test(() => {
        const expected = i === 0x00 || (scheme === "https" && i === 0x1F) ? "host" : stripped ? "test" : expectedPart;
        const url = urlRecord(scheme);
        url.hostname = input;
        assert_equals(url.hostname, expected, "property");
        assert_equals(url.href, urlString({ scheme, host: expected }), "href");
      }, `Setting hostname with ${type} ${cpReference} (${scheme}:)`);
    }

    test(() => {
      const expected = stripped ? "9000" : "8000";
      const url = urlRecord(scheme);
      url.port = String.fromCodePoint(i) + "9000";
      assert_equals(url.port, expected, "property");
      assert_equals(url.href, urlString({ scheme, port: expected }), "href");
    }, `Setting port with leading ${cpReference} (${scheme}:)`);

    test(() => {
      const expected = stripped ? "9000" : "90";
      const url = urlRecord(scheme);
      url.port = "90" + String.fromCodePoint(i) + "00";
      assert_equals(url.port, expected, "property");
      assert_equals(url.href, urlString({ scheme, port: expected }), "href");
    }, `Setting port with middle ${cpReference} (${scheme}:)`);

    test(() => {
      const expected = "9000";
      const url = urlRecord(scheme);
      url.port = "9000" + String.fromCodePoint(i);
      assert_equals(url.port, expected, "property");
      assert_equals(url.href, urlString({ scheme, port: expected }), "href");
    }, `Setting port with trailing ${cpReference} (${scheme}:)`);

    for (const [property, separator] of [["pathname", "/"], ["search", "?"], ["hash", "#"]]) {
      for (const [type, expectedPart, input] of [
        ["leading", encodeURIComponent(cpString) + "test", String.fromCodePoint(i) + "test"],
        ["middle", "te" + encodeURIComponent(cpString) + "st", "te" + String.fromCodePoint(i) + "st"],
        ["trailing", "test" + encodeURIComponent(cpString), "test" + String.fromCodePoint(i)]
      ]) {
        test(() => {
          const expected = stripped ? "test" : expectedPart;
          const url = urlRecord(scheme);
          url[property] = input;
          assert_equals(url[property], separator + expected, "property");
          assert_equals(url.href, urlString({ scheme, [property]: expected }), "href");
        }, `Setting ${property} with ${type} ${cpReference} (${scheme}:)`);
      }
    }
  }
}
