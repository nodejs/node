# Policies

<!--introduced_in=v11.8.0-->
<!-- type=misc -->

> Stability: 1 - Experimental

<!-- name=policy -->

Node.js contains experimental support for creating policies on loading code.

Policies are a security feature intended to allow guarantees
about what code Node.js is able to load. The use of policies assumes
safe practices for the policy files such as ensuring that policy
files cannot be overwritten by the Node.js application by using
file permissions.

A best practice would be to ensure that the policy manifest is read only for
the running Node.js application, and that the file cannot be changed
by the running Node.js application in any way. A typical setup would be to
create the policy file as a different user id than the one running Node.js
and granting read permissions to the user id running Node.js.

## Enabling

<!-- type=misc -->

The `--experimental-policy` flag can be used to enable features for policies
when loading modules.

Once this has been set, all modules must conform to a policy manifest file
passed to the flag:

```sh
node --experimental-policy=policy.json app.js
```

The policy manifest will be used to enforce constraints on code loaded by
Node.js.

## Features

### Error Behavior

When a policy check fails, Node.js by default will throw an error.
It is possible to change the error behavior to one of a few possibilities
by defining an "onerror" field in a policy manifest. The following values are
available to change the behavior:

* `"exit"` - will exit the process immediately.
    No cleanup code will be allowed to run.
* `"log"` - will log the error at the site of the failure.
* `"throw"` (default) - will throw a JS error at the site of the failure.

```json
{
  "onerror": "log",
  "resources": {
    "./app/checked.js": {
      "integrity": "sha384-SggXRQHwCG8g+DktYYzxkXRIkTiEYWBHqev0xnpCxYlqMBufKZHAHQM3/boDaI/0"
    }
  }
}
```

### Integrity Checks

Policy files must use integrity checks with Subresource Integrity strings
compatible with the browser
[integrity attribute](https://www.w3.org/TR/SRI/#the-integrity-attribute)
associated with absolute URLs.

When using `require()` all resources involved in loading are checked for
integrity if a policy manifest has been specified. If a resource does not match
the integrity listed in the manifest, an error will be thrown.

An example policy file that would allow loading a file `checked.js`:

```json
{
  "resources": {
    "./app/checked.js": {
      "integrity": "sha384-SggXRQHwCG8g+DktYYzxkXRIkTiEYWBHqev0xnpCxYlqMBufKZHAHQM3/boDaI/0"
    }
  }
}
```

Each resource listed in the policy manifest can be of one the following
formats to determine its location:

1. A [relative url string][] to a resource from the manifest such as `./resource.js`, `../resource.js`, or `/resource.js`.
2. A complete url string to a resource such as `file:///resource.js`.

When loading resources the entire URL must match including search parameters
and hash fragment. `./a.js?b` will not be used when attempting to load
`./a.js` and vice versa.

In order to generate integrity strings, a script such as
`printf "sha384-$(cat checked.js | openssl dgst -sha384 -binary | base64)"`
can be used.


[relative url string]: https://url.spec.whatwg.org/#relative-url-with-fragment-string
