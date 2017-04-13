# Troubleshooting Common Issues

## Using this Document

Search for the error message you're getting and see if there's a match, or skim the [table of contents](#table-of-contents) below for topics that seem relevant to the issue you're having. Each issue section has steps to work around or fix the particular issue, and have examples of common error messages.

If you do not find the issue below, try searching the issue tracker itself for potential duplicates before opening a new issue.

If you're reading this document because you noticed an issue with npm's web site, please let the [web team](https://github.com/npm/www/issues) know.

### Updating this Document

If you think something should be added here, make a PR that includes the following:

0. a summary
0. one or more example errors
0. steps to debug and fix
0. links to at least one related issue from the tracker

For more details of the content and formatting of these entries, refer to examples below.

## Table of Contents

* [Upgrading npm](#upgrading-npm)
* [Proxies and Networking](#proxy-and-networking-issues)
* [Cannot find module](#cannot-find-module)
* [Shasum Check Fails](#shasum-check-fails)
* [No Git](#no-git)

## Upgrading npm

Whenever you get npm errors, it's a good idea to first check your npm version and upgrade to latest whenever possible. We still see people running npm@1 (!) and in those cases, installing the latest version of npm usually solves the problem.

You can check your npm version by running `npm -v`.

### Steps to Fix
* Upgrading on \*nix (OSX, Linux, etc.)

(You may need to prefix these commands with sudo, especially on Linux, or OS X if you installed Node using its default installer.)
You can upgrade to the latest version of npm using:
`npm install -g npm@latest`
Or upgrade to the most recent LTS release:
`npm install -g npm@lts`

* Upgrading on Windows

We have a [detailed guide](https://github.com/npm/npm/wiki/Troubleshooting#upgrading-on-windows) for upgrading on windows on our wiki.

## Proxy and Networking Issues

npm might not be able to connect to the registry for various reasons. Perhaps your machine is behind a firewall that needs to be opened, or you require a corporate proxy to access the npm registry. This issue can manifest in a wide number of different ways. Usually, strange network errors are an instance of this specific problem.

Sometimes, users may have install failures due to Git/Github access issues. Git/GitHub access is separate from npm registry access. For users in some locations (India in particular), problems installing packages may be due to connectivity problems reaching GitHub and not the npm registry.

If you believe your network is configured and working correctly, and you're still having problems installing, please let the [registry team](https://github.com/npm/registry/issues) know you're having trouble.

### Steps to Fix

0. Make sure you have a working internet connection. Can you reach https://registry.npmjs.org? Can you reach other sites? If other sites are unreachable, this is not a problem with npm.

0. Check http://status.npmjs.org/ for any potential current service outages.

0. If your company has a process for domain whitelisting for developers, make sure https://registry.npmjs.org is a whitelisted domain.

0. If you're in China, consider using https://npm.taobao.org/ as a registry, which sits behind the Firewall.

0. On Windows, npm does not access proxies configured at the system level, so you need to configure them manually in order for npm to access them. Make sure [you have added the appropriate proxy configuration to `.npmrc`](https://docs.npmjs.com/misc/config#https-proxy).

0. If you already have a proxy configured, it might be configured incorrectly or use the wrong credentials. Verify your credentials, test the specific credentials with a separate application.

0. The proxy itself, on the server, might also have a configuration error. In this case, you'll need to work with your system administrator to verify that the proxy, and HTTPS, are configured correctly. You may test it by running regular HTTPS requests.

### Example Errors

This error can manifest in a wide range of different ways:

```
npm ERR! code UNABLE_TO_VERIFY_LEAF_SIGNATURE
npm ERR! unable to verify the first certificate
```
```
npm ERR! code UNABLE_TO_GET_ISSUER_CERT_LOCALLY
npm ERR! unable to get local issuer certificate
```
```
npm ERR! code DEPTH_ZERO_SELF_SIGNED_CERT
npm ERR! self signed certificate
```
```
124 error code ECONNREFUSED
125 error errno ECONNREFUSED
126 error syscall connect
```
```
136 error Unexpected token <
136 error <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
136 error <HTML><HEAD><META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=iso-8859-1">
136 error <TITLE>ERROR: Cache Access Denied</TITLE>
```
```
31 verbose stack Error: connect ETIMEDOUT 123.123.123.123:443
```
```
108 error code EAI_AGAIN
109 error errno EAI_AGAIN
110 error syscall getaddrinfo
111 error getaddrinfo EAI_AGAIN proxy.yourcorp.com:811
```
```
npm ERR! Error: getaddrinfo ESRCH
npm ERR! at errnoException (dns.js:37:11)
npm ERR! at Object.onanswer as oncomplete
```
```
35 error Unexpected token u
35 error function FindProxyForURL(url, host) {
```

### Related issues
* [#14318](https://github.com/npm/npm/issues/14318)
* [#15059](https://github.com/npm/npm/issues/15059)
* [#14336](https://github.com/npm/npm/issues/14336)

## Cannot find module

If *when running npm* (not your application), you get an error about a module not being found, this almost certainly means that there's something wrong with your npm installation.

If this happens when trying to start your application, you might not have installed your package's dependencies yet.

### Steps to Fix

0. If this happens when you try to start your application, try running `npm install` to install the app's dependencies. Make sure all its actual dependencies are listed in `package.json`

0. If this happens on any npm command, please reinstall.

### Examples

```
module.js:338
    throw err;
          ^
Error: Cannot find module
```
### Related Issues

* [#14699](https://github.com/npm/npm/issues/14699)

## Shasum Check Fails

This is a common issue which used to be caused by caching issues. Nowadays, the cache has been improved, so it's likely to be an install issue, which can be caused by network problems (sometimes even [proxy issues](#proxy-and-networking-issues)), a node bug, or possibly some sort of npm bug.

### Steps to Fix

0. Try running `npm install` again. It may have been a momentary hiccup or corruption during package download.

0. Check http://status.npmjs.org/ for any potential current service outages.

0. If the shasum error specifically has `Actual: da39a3ee5e6b4b0d3255bfef95601890afd80709`, with this exact shasum, it means the package download was empty, which is certainly a networking issue.

0. Make sure your [network connection and proxy settings](#proxy-and-networking-issues) are ok.

0. Update your node version to the latest stable version.

### Examples

```
npm ERR! shasum check failed for C:\Users\some-user\AppData\Local\Temp\npm-9356-7
d74e411\registry.npmjs.org\some-package\-\some-package-1.0.0.tgz
npm ERR! Expected: 652294c14651db29fa93bd2d5ff2983a4f08c636
npm ERR! Actual:   c45474b40e6a7474633ec6f2b0315feaf15c61f2
npm ERR! From:     https://registry.npmjs.org/some-package/-/some-package-1.0.0.tgz
```

### Related Issues
* [#14720](https://github.com/npm/npm/issues/14720)
* [#13405](https://github.com/npm/npm/issues/13405)

## No Git
If your install fails and you see a message saying you don't have git installed, it should be resolved by installing git.

### Steps to Fix

0. [Install git](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) following the instructions for your machine.

### Examples

npm ERR! not found: git
ENOGIT

### Related Issues

* [#11095](https://github.com/npm/npm/issues/11095)
