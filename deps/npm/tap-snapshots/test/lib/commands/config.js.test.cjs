/* IMPORTANT
 * This snapshot file is auto-generated, but designed for humans.
 * It should be checked into source control and tracked carefully.
 * Re-generate by setting TAP_SNAPSHOT=1 and running tests.
 * Make sure to inspect the output below.  Do not ignore changes!
 */
'use strict'
exports[`test/lib/commands/config.js TAP config list --json > output matches snapshot 1`] = `
{
  "prefix": "{LOCALPREFIX}",
  "userconfig": "{HOME}/.npmrc",
  "cache": "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list---json-sandbox/cache",
  "json": true,
  "projectloaded": "yes",
  "userloaded": "yes",
  "globalloaded": "yes",
  "access": null,
  "all": false,
  "allow-same-version": false,
  "also": null,
  "audit": true,
  "audit-level": null,
  "auth-type": "legacy",
  "before": null,
  "bin-links": true,
  "browser": null,
  "ca": null,
  "cache-max": null,
  "cache-min": 0,
  "cafile": null,
  "call": "",
  "cert": null,
  "ci-name": null,
  "cidr": null,
  "color": true,
  "commit-hooks": true,
  "depth": null,
  "description": true,
  "dev": false,
  "diff": [],
  "diff-ignore-all-space": false,
  "diff-name-only": false,
  "diff-no-prefix": false,
  "diff-dst-prefix": "b/",
  "diff-src-prefix": "a/",
  "diff-text": false,
  "diff-unified": 3,
  "dry-run": false,
  "editor": "{EDITOR}",
  "engine-strict": false,
  "fetch-retries": 2,
  "fetch-retry-factor": 10,
  "fetch-retry-maxtimeout": 60000,
  "fetch-retry-mintimeout": 10000,
  "fetch-timeout": 300000,
  "force": false,
  "foreground-scripts": false,
  "format-package-lock": true,
  "fund": true,
  "git": "git",
  "git-tag-version": true,
  "global": false,
  "global-style": false,
  "globalconfig": "{GLOBALPREFIX}/npmrc",
  "heading": "npm",
  "https-proxy": null,
  "if-present": false,
  "ignore-scripts": false,
  "include": [],
  "include-staged": false,
  "include-workspace-root": false,
  "init-author-email": "",
  "init-author-name": "",
  "init-author-url": "",
  "init-license": "ISC",
  "init-module": "{HOME}/.npm-init.js",
  "init-version": "1.0.0",
  "init.author.email": "",
  "init.author.name": "",
  "init.author.url": "",
  "init.license": "ISC",
  "init.module": "{HOME}/.npm-init.js",
  "init.version": "1.0.0",
  "install-links": false,
  "key": null,
  "legacy-bundling": false,
  "legacy-peer-deps": false,
  "link": false,
  "local-address": null,
  "location": "user",
  "lockfile-version": null,
  "loglevel": "notice",
  "logs-dir": null,
  "logs-max": 10,
  "long": false,
  "maxsockets": 15,
  "message": "%s",
  "node-options": null,
  "node-version": "{NODE-VERSION}",
  "noproxy": [
    ""
  ],
  "npm-version": "{NPM-VERSION}",
  "offline": false,
  "omit": [],
  "omit-lockfile-registry-resolved": false,
  "only": null,
  "optional": null,
  "otp": null,
  "package": [],
  "package-lock": true,
  "package-lock-only": false,
  "pack-destination": ".",
  "parseable": false,
  "prefer-offline": false,
  "prefer-online": false,
  "preid": "",
  "production": null,
  "progress": true,
  "proxy": null,
  "read-only": false,
  "rebuild-bundle": true,
  "registry": "https://registry.npmjs.org/",
  "save": true,
  "save-bundle": false,
  "save-dev": false,
  "save-exact": false,
  "save-optional": false,
  "save-peer": false,
  "save-prefix": "^",
  "save-prod": false,
  "scope": "",
  "script-shell": null,
  "searchexclude": "",
  "searchlimit": 20,
  "searchopts": "",
  "searchstaleness": 900,
  "shell": "{SHELL}",
  "shrinkwrap": true,
  "sign-git-commit": false,
  "sign-git-tag": false,
  "sso-poll-frequency": 500,
  "sso-type": "oauth",
  "strict-peer-deps": false,
  "strict-ssl": true,
  "tag": "latest",
  "tag-version-prefix": "v",
  "timing": false,
  "tmp": "{TMP}",
  "umask": 0,
  "unicode": false,
  "update-notifier": true,
  "usage": false,
  "user-agent": "npm/{npm-version} node/{node-version} {platform} {arch} workspaces/{workspaces} {ci}",
  "version": false,
  "versions": false,
  "viewer": "{VIEWER}",
  "which": null,
  "workspace": [],
  "workspaces": null,
  "workspaces-update": true,
  "yes": null,
  "metrics-registry": "https://registry.npmjs.org/"
}
`

exports[`test/lib/commands/config.js TAP config list --long > output matches snapshot 1`] = `
; "default" config from default values

_auth = (protected)
access = null
all = false
allow-same-version = false
also = null
audit = true
audit-level = null
auth-type = "legacy"
before = null
bin-links = true
browser = null
ca = null
; cache = "{CACHE}" ; overridden by cli
cache-max = null
cache-min = 0
cafile = null
call = ""
cert = null
ci-name = null
cidr = null
color = true
commit-hooks = true
depth = null
description = true
dev = false
diff = []
diff-dst-prefix = "b/"
diff-ignore-all-space = false
diff-name-only = false
diff-no-prefix = false
diff-src-prefix = "a/"
diff-text = false
diff-unified = 3
dry-run = false
editor = "{EDITOR}"
engine-strict = false
fetch-retries = 2
fetch-retry-factor = 10
fetch-retry-maxtimeout = 60000
fetch-retry-mintimeout = 10000
fetch-timeout = 300000
force = false
foreground-scripts = false
format-package-lock = true
fund = true
git = "git"
git-tag-version = true
global = false
global-style = false
globalconfig = "{GLOBALPREFIX}/npmrc"
heading = "npm"
https-proxy = null
if-present = false
ignore-scripts = false
include = []
include-staged = false
include-workspace-root = false
init-author-email = ""
init-author-name = ""
init-author-url = ""
init-license = "ISC"
init-module = "{HOME}/.npm-init.js"
init-version = "1.0.0"
init.author.email = ""
init.author.name = ""
init.author.url = ""
init.license = "ISC"
init.module = "{HOME}/.npm-init.js"
init.version = "1.0.0"
install-links = false
json = false
key = null
legacy-bundling = false
legacy-peer-deps = false
link = false
local-address = null
location = "user"
lockfile-version = null
loglevel = "notice"
logs-dir = null
logs-max = 10
; long = false ; overridden by cli
maxsockets = 15
message = "%s"
metrics-registry = "https://registry.npmjs.org/"
node-options = null
node-version = "{NODE-VERSION}"
noproxy = [""]
npm-version = "{NPM-VERSION}"
offline = false
omit = []
omit-lockfile-registry-resolved = false
only = null
optional = null
otp = null
pack-destination = "."
package = []
package-lock = true
package-lock-only = false
parseable = false
prefer-offline = false
prefer-online = false
; prefix = "{REALGLOBALREFIX}" ; overridden by cli
preid = ""
production = null
progress = true
proxy = null
read-only = false
rebuild-bundle = true
registry = "https://registry.npmjs.org/"
save = true
save-bundle = false
save-dev = false
save-exact = false
save-optional = false
save-peer = false
save-prefix = "^"
save-prod = false
scope = ""
script-shell = null
searchexclude = ""
searchlimit = 20
searchopts = ""
searchstaleness = 900
shell = "{SHELL}"
shrinkwrap = true
sign-git-commit = false
sign-git-tag = false
sso-poll-frequency = 500
sso-type = "oauth"
strict-peer-deps = false
strict-ssl = true
tag = "latest"
tag-version-prefix = "v"
timing = false
tmp = "{TMP}"
umask = 0
unicode = false
update-notifier = true
usage = false
user-agent = "npm/{npm-version} node/{node-version} {platform} {arch} workspaces/{workspaces} {ci}"
; userconfig = "{HOME}/.npmrc" ; overridden by cli
version = false
versions = false
viewer = "{VIEWER}"
which = null
workspace = []
workspaces = null
workspaces-update = true
yes = null

; "global" config from {GLOBALPREFIX}/npmrc

globalloaded = "yes"

; "user" config from {HOME}/.npmrc

userloaded = "yes"

; "project" config from {LOCALPREFIX}/.npmrc

projectloaded = "yes"

; "cli" config from command line options

cache = "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list---long-sandbox/cache"
long = true
prefix = "{LOCALPREFIX}"
userconfig = "{HOME}/.npmrc"
`

exports[`test/lib/commands/config.js TAP config list > output matches snapshot 1`] = `
; "cli" config from command line options

cache = "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list-sandbox/cache"
prefix = "{LOCALPREFIX}"
userconfig = "{HOME}/.npmrc"

; node bin location = {EXECPATH}
; node version = {NODE-VERSION}
; npm local prefix = {LOCALPREFIX}
; npm version = {NPM-VERSION}
; cwd = {NPMDIR}
; HOME = {HOME}
; Run \`npm config ls -l\` to show all defaults.
`

exports[`test/lib/commands/config.js TAP config list with publishConfig > output matches snapshot 1`] = `
; "cli" config from command line options

cache = "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list-with-publishConfig-sandbox/cache"
prefix = "{LOCALPREFIX}"
userconfig = "{HOME}/.npmrc"

; node bin location = {EXECPATH}
; node version = {NODE-VERSION}
; npm local prefix = {LOCALPREFIX}
; npm version = {NPM-VERSION}
; cwd = {NPMDIR}
; HOME = {HOME}
; Run \`npm config ls -l\` to show all defaults.

; "publishConfig" from {LOCALPREFIX}/package.json
; This set of config values will be used at publish-time.

_authToken = (protected)
registry = "https://some.registry"
; "env" config from environment

; cache = "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list-with-publishConfig-sandbox/cache" ; overridden by cli
global-prefix = "{LOCALPREFIX}"
globalconfig = "{GLOBALPREFIX}/npmrc"
init-module = "{HOME}/.npm-init.js"
local-prefix = "{LOCALPREFIX}"
; prefix = "{LOCALPREFIX}" ; overridden by cli
user-agent = "npm/{NPM-VERSION} node/{NODE-VERSION} {PLATFORM} {ARCH} workspaces/false"
; userconfig = "{HOME}/.npmrc" ; overridden by cli

; "cli" config from command line options

cache = "{NPMDIR}/test/lib/commands/tap-testdir-config-config-list-with-publishConfig-sandbox/cache"
global = true
prefix = "{LOCALPREFIX}"
userconfig = "{HOME}/.npmrc"

; node bin location = {EXECPATH}
; node version = {NODE-VERSION}
; npm local prefix = {LOCALPREFIX}
; npm version = {NPM-VERSION}
; cwd = {NPMDIR}
; HOME = {HOME}
; Run \`npm config ls -l\` to show all defaults.
`
