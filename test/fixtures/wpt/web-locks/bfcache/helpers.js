export function runWebLocksBfcacheTest(params, description) {
  runBfcacheTest(
    {
      scripts: ["/web-locks/resources/helpers.js"],
      openFunc: url =>
        window.open(
          url + `&prefix=${location.pathname}-${description}`,
          "_blank",
          "noopener"
        ),
      ...params,
    },
    description
  );
}
