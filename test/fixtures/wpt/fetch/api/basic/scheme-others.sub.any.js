// META: global=window,worker
// META: script=../resources/utils.js

function checkKoUrl(url, desc) {
  if (!desc)
    desc = "Fetching " + url.substring(0, 45) + " is KO"
  promise_test(function(test) {
    var promise = fetch(url);
    return promise_rejects_js(test, TypeError, promise);
  }, desc);
}

var urlWithoutScheme = "://{{host}}:{{ports[http][0]}}/";
checkKoUrl("aaa" + urlWithoutScheme);
checkKoUrl("cap" + urlWithoutScheme);
checkKoUrl("cid" + urlWithoutScheme);
checkKoUrl("dav" + urlWithoutScheme);
checkKoUrl("dict" + urlWithoutScheme);
checkKoUrl("dns" + urlWithoutScheme);
checkKoUrl("geo" + urlWithoutScheme);
checkKoUrl("im" + urlWithoutScheme);
checkKoUrl("imap" + urlWithoutScheme);
checkKoUrl("ipp" + urlWithoutScheme);
checkKoUrl("ldap" + urlWithoutScheme);
checkKoUrl("mailto" + urlWithoutScheme);
checkKoUrl("nfs" + urlWithoutScheme);
checkKoUrl("pop" + urlWithoutScheme);
checkKoUrl("rtsp" + urlWithoutScheme);
checkKoUrl("snmp" + urlWithoutScheme);

done();
