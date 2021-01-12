// META: script=/common/get-host-info.sub.js

promise_test(t => {
  const img = new Image();
  img.src = get_host_info().HTTP_REMOTE_ORIGIN + "/fetch/cross-origin-resource-policy/resources/image.py?corp=same-site";
  return new Promise((resolve, reject) => {
    img.onload = resolve;
    img.onerror = reject;
    document.body.appendChild(img);
  }).finally(() => {
    img.remove();
  });
}, "Cross-Origin-Resource-Policy does not block Mixed Content <img>");
