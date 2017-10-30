# npm-registry-client

The code that npm uses to talk to the registry.

It handles all the caching and HTTP calls.

## Usage

```javascript
var RegClient = require('npm-registry-client')
var client = new RegClient(config)
var uri = "https://registry.npmjs.org/npm"
var params = {timeout: 1000}

client.get(uri, params, function (error, data, raw, res) {
  // error is an error if there was a problem.
  // data is the parsed data object
  // raw is the json string
  // res is the response from couch
})
```

# Registry URLs

The registry calls take either a full URL pointing to a resource in the
registry, or a base URL for the registry as a whole (including the registry
path – but be sure to terminate the path with `/`). `http` and `https` URLs are
the only ones supported.

## Using the client

Every call to the client follows the same pattern:

* `uri` {String} The *fully-qualified* URI of the registry API method being
  invoked.
* `params` {Object} Per-request parameters.
* `callback` {Function} Callback to be invoked when the call is complete.

### Credentials

Many requests to the registry can be authenticated, and require credentials
for authorization. These credentials always look the same:

* `username` {String}
* `password` {String}
* `email` {String}
* `alwaysAuth` {Boolean} Whether calls to the target registry are always
  authed.

**or**

* `token` {String}
* `alwaysAuth` {Boolean} Whether calls to the target registry are always
  authed.

## Requests

As of `npm-registry-client@8`, all requests are made with an `Accept` header
of `application/vnd.npm.install-v1+json; q=1.0, application/json; q=0.8, */*`.

This enables filtered document responses to requests for package metadata.
You know that you got a filtered response if the mime type is set to
`application/vnd.npm.install-v1+json` and not `application/json`.

This filtering substantially reduces the over all data size.  For example
for `https://registry.npmjs.org/npm`, the compressed metadata goes from
410kB to 21kB.

## API

### client.access(uri, params, cb)

* `uri` {String} Registry URL for the package's access API endpoint.
  Looks like `/-/package/<package name>/access`.
* `params` {Object} Object containing per-request properties.
  * `access` {String} New access level for the package. Can be either
    `public` or `restricted`. Registry will raise an error if trying
    to change the access level of an unscoped package.
  * `auth` {Credentials}

Set the access level for scoped packages. For now, there are only two
access levels: "public" and "restricted".

### client.adduser(uri, params, cb)

* `uri` {String} Base registry URL.
* `params` {Object} Object containing per-request properties.
  * `auth` {Credentials}
* `cb` {Function}
  * `error` {Error | null}
  * `data` {Object} the parsed data object
  * `raw` {String} the json
  * `res` {Response Object} response from couch

Add a user account to the registry, or verify the credentials.

### client.deprecate(uri, params, cb)

* `uri` {String} Full registry URI for the deprecated package.
* `params` {Object} Object containing per-request properties.
  * `version` {String} Semver version range.
  * `message` {String} The message to use as a deprecation warning.
  * `auth` {Credentials}
* `cb` {Function}

Deprecate a version of a package in the registry.

### client.distTags.fetch(uri, params, cb)

* `uri` {String} Base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `package` {String} Name of the package.
  * `auth` {Credentials}
* `cb` {Function}

Fetch all of the `dist-tags` for the named package.

### client.distTags.add(uri, params, cb)

* `uri` {String} Base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `package` {String} Name of the package.
  * `distTag` {String} Name of the new `dist-tag`.
  * `version` {String} Exact version to be mapped to the `dist-tag`.
  * `auth` {Credentials}
* `cb` {Function}

Add (or replace) a single dist-tag onto the named package.

### client.distTags.set(uri, params, cb)

* `uri` {String} Base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `package` {String} Name of the package.
  * `distTags` {Object} Object containing a map from tag names to package
     versions.
  * `auth` {Credentials}
* `cb` {Function}

Set all of the `dist-tags` for the named package at once, creating any
`dist-tags` that do not already exist. Any `dist-tags` not included in the
`distTags` map will be removed.

### client.distTags.update(uri, params, cb)

* `uri` {String} Base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `package` {String} Name of the package.
  * `distTags` {Object} Object containing a map from tag names to package
     versions.
  * `auth` {Credentials}
* `cb` {Function}

Update the values of multiple `dist-tags`, creating any `dist-tags` that do
not already exist. Any pre-existing `dist-tags` not included in the `distTags`
map will be left alone.

### client.distTags.rm(uri, params, cb)

* `uri` {String} Base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `package` {String} Name of the package.
  * `distTag` {String} Name of the new `dist-tag`.
  * `auth` {Credentials}
* `cb` {Function}

Remove a single `dist-tag` from the named package.

### client.get(uri, params, cb)

* `uri` {String} The complete registry URI to fetch
* `params` {Object} Object containing per-request properties.
  * `timeout` {Number} Duration before the request times out. Optional
    (default: never).
  * `follow` {Boolean} Follow 302/301 responses. Optional (default: true).
  * `staleOk` {Boolean} If there's cached data available, then return that to
    the callback quickly, and update the cache the background. Optional
    (default: false).
  * `auth` {Credentials} Optional.
  * `fullMetadata` {Boolean} If true, don't attempt to fetch filtered
    ("corgi") registry metadata.  (default: false)
* `cb` {Function}

Fetches data from the registry via a GET request, saving it in the cache folder
with the ETag or the "Last Modified" timestamp.

### client.publish(uri, params, cb)

* `uri` {String} The registry URI for the package to publish.
* `params` {Object} Object containing per-request properties.
  * `metadata` {Object} Package metadata.
  * `access` {String} Access for the package. Can be `public` or `restricted` (no default).
  * `body` {Stream} Stream of the package body / tarball.
  * `auth` {Credentials}
* `cb` {Function}

Publish a package to the registry.

Note that this does not create the tarball from a folder.

### client.sendAnonymousCLIMetrics(uri, params, cb)

- `uri` {String} Base URL for the registry.
- `params` {Object} Object containing per-request properties.
  - `metricId` {String} A uuid unique to this dataset.
  - `metrics` {Object} The metrics to share with the registry, with the following properties:
    - `from` {Date} When the first data in this report was collected.
    - `to` {Date} When the last data in this report was collected. Usually right now.
    - `successfulInstalls` {Number} The number of successful installs in this period.
    - `failedInstalls` {Number} The number of installs that ended in error in this period.
- `cb` {Function}

PUT a metrics object to the `/-/npm/anon-metrics/v1/` endpoint on the registry.

### client.star(uri, params, cb)

* `uri` {String} The complete registry URI for the package to star.
* `params` {Object} Object containing per-request properties.
  * `starred` {Boolean} True to star the package, false to unstar it. Optional
    (default: false).
  * `auth` {Credentials}
* `cb` {Function}

Star or unstar a package.

Note that the user does not have to be the package owner to star or unstar a
package, though other writes do require that the user be the package owner.

### client.stars(uri, params, cb)

* `uri` {String} The base URL for the registry.
* `params` {Object} Object containing per-request properties.
  * `username` {String} Name of user to fetch starred packages for. Optional
    (default: user in `auth`).
  * `auth` {Credentials} Optional (required if `username` is omitted).
* `cb` {Function}

View your own or another user's starred packages.

### client.tag(uri, params, cb)

* `uri` {String} The complete registry URI to tag
* `params` {Object} Object containing per-request properties.
  * `version` {String} Version to tag.
  * `tag` {String} Tag name to apply.
  * `auth` {Credentials}
* `cb` {Function}

Mark a version in the `dist-tags` hash, so that `pkg@tag` will fetch the
specified version.

### client.unpublish(uri, params, cb)

* `uri` {String} The complete registry URI of the package to unpublish.
* `params` {Object} Object containing per-request properties.
  * `version` {String} version to unpublish. Optional – omit to unpublish all
    versions.
  * `auth` {Credentials}
* `cb` {Function}

Remove a version of a package (or all versions) from the registry.  When the
last version us unpublished, the entire document is removed from the database.

### client.whoami(uri, params, cb)

* `uri` {String} The base registry for the URI.
* `params` {Object} Object containing per-request properties.
  * `auth` {Credentials}
* `cb` {Function}

Simple call to see who the registry thinks you are. Especially useful with
token-based auth.


## PLUMBING

The below are primarily intended for use by the rest of the API, or by the npm
caching logic directly.

### client.request(uri, params, cb)

* `uri` {String} URI pointing to the resource to request.
* `params` {Object} Object containing per-request properties.
  * `method` {String} HTTP method. Optional (default: "GET").
  * `body` {Stream | Buffer | String | Object} The request body.  Objects
    that are not Buffers or Streams are encoded as JSON. Optional – body
    only used for write operations.
  * `etag` {String} The cached ETag. Optional.
  * `lastModified` {String} The cached Last-Modified timestamp. Optional.
  * `follow` {Boolean} Follow 302/301 responses. Optional (default: true).
  * `streaming` {Boolean} Stream the request body as it comes, handling error
    responses in a non-streaming way.
  * `auth` {Credentials} Optional.
* `cb` {Function}
  * `error` {Error | null}
  * `data` {Object} the parsed data object
  * `raw` {String} the json
  * `res` {Response Object} response from couch

Make a generic request to the registry. All the other methods are wrappers
around `client.request`.

### client.fetch(uri, params, cb)

* `uri` {String} The complete registry URI to upload to
* `params` {Object} Object containing per-request properties.
  * `headers` {Stream} HTTP headers to be included with the request. Optional.
  * `auth` {Credentials} Optional.
* `cb` {Function}

Fetch a package from a URL, with auth set appropriately if included. Used to
cache remote tarballs as well as request package tarballs from the registry.

# Configuration

The client uses its own configuration, which is just passed in as a simple
nested object. The following are the supported values (with their defaults, if
any):

* `proxy.http` {URL} The URL to proxy HTTP requests through.
* `proxy.https` {URL} The URL to proxy HTTPS requests through. Defaults to be
  the same as `proxy.http` if unset.
* `proxy.localAddress` {IP} The local address to use on multi-homed systems.
* `ssl.ca` {String} Certificate signing authority certificates to trust.
* `ssl.certificate` {String} Client certificate (PEM encoded). Enable access
  to servers that require client certificates.
* `ssl.key` {String} Private key (PEM encoded) for client certificate.
* `ssl.strict` {Boolean} Whether or not to be strict with SSL certificates.
  Default = `true`
* `retry.count` {Number} Number of times to retry on GET failures. Default = 2.
* `retry.factor` {Number} `factor` setting for `node-retry`. Default = 10.
* `retry.minTimeout` {Number} `minTimeout` setting for `node-retry`.
  Default = 10000 (10 seconds)
* `retry.maxTimeout` {Number} `maxTimeout` setting for `node-retry`.
  Default = 60000 (60 seconds)
* `userAgent` {String} User agent header to send. Default =
  `"node/{process.version}"`
* `log` {Object} The logger to use.  Defaults to `require("npmlog")` if
  that works, otherwise logs are disabled.
* `defaultTag` {String} The default tag to use when publishing new packages.
  Default = `"latest"`
* `couchToken` {Object} A token for use with
  [couch-login](https://npmjs.org/package/couch-login).
* `sessionToken` {String} A random identifier for this set of client requests.
  Default = 8 random hexadecimal bytes.
* `maxSockets` {Number} The maximum number of connections that will be open per
  origin (unique combination of protocol:host:port). Passed to the
  [httpAgent](https://nodejs.org/api/http.html#http_agent_maxsockets).
  Default = 50
* `isFromCI` {Boolean} Identify to severs if this request is coming from CI (for statistics purposes).
  Default = detected from environment– primarily this is done by looking for
  the CI environment variable to be set to `true`.  Also accepted are the
  existence of the `JENKINS_URL`, `bamboo.buildKey` and `TDDIUM` environment
  variables.
* `scope` {String} The scope of the project this command is being run for. This is the
  top level npm module in which a command was run.
  Default = none
