[![Build Status](https://travis-ci.org/robertkowalski/github-url-from-username-repo.png?branch=master)](https://travis-ci.org/robertkowalski/github-url-from-username-repo)
[![Dependency Status](https://gemnasium.com/robertkowalski/github-url-from-username-repo.png)](https://gemnasium.com/robertkowalski/github-url-from-username-repo)


# github-url-from-username-repo

## API

### getUrl(url, [forBrowser])

Get's the url normalized for npm.
If `forBrowser` is true, return a GitHub url that is usable in a webbrowser.

## Usage

```javascript

var getUrl = require("github-url-from-username-repo")
getUrl("visionmedia/express") // https://github.com/visionmedia/express

```
