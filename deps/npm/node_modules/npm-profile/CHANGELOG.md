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
