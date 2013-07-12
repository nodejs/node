module.exports = isGitUrl

function isGitUrl (url) {
  switch (url.protocol) {
    case "git:":
    case "git+http:":
    case "git+https:":
    case "git+rsync:":
    case "git+ftp:":
    case "git+ssh:":
      return true
  }
}
