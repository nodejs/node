# v4.0.1 (2018-08-29)

- `opts.password` needs to be base64-encoded when passed in for login
- Bump `npm-registry-fetch` dep because we depend on `opts.forceAuth`

# v4.0.0 (2018-08-28)

## BREAKING CHANGES:

- Networking and auth-related options now use the latest [`npm-registry-fetch` config format](https://www.npmjs.com/package/npm-registry-fetch#fetch-opts).

# v3.0.2 (2018-06-07)

- Allow newer make-fetch-happen.
- Report 500s from weblogin end point as unsupported.
- EAUTHUNKNOWN errors were incorrectly reported as EAUTHIP.

# v3.0.1 (2018-02-18)

- Log `npm-notice` headers

# v3.0.0 (2018-02-18)

## BREAKING CHANGES:

- profile.login() and profile.adduser() take 2 functions: opener() and
  prompter().  opener is used when we get the url couplet from the
  registry.  prompter is used if web-based login fails.
- Non-200 status codes now always throw.  Previously if the `content.error`
  property was set, `content` would be returned. Content is available on the
  thrown error object in the `body` property.

## FEATURES:

- The previous adduser is available as adduserCouch
- The previous login is available as loginCouch
- New loginWeb and adduserWeb commands added, which take an opener
  function to open up the web browser.
- General errors have better error message reporting

## FIXES:

- General errors now correctly include the URL.
- Missing user errors from Couch are now thrown. (As was always intended.)
- Many errors have better stacktrace filtering.
