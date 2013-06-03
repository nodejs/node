exports.email = email
exports.pw = pw
exports.username = username

function username (un) {
  if (un !== un.toLowerCase()) {
    return new Error('Username must be lowercase')
  }

  if (un !== encodeURIComponent(un)) {
    return new Error('Username may not contain non-url-safe chars')
  }

  if (un.charAt(0) === '.') {
    return new Error('Username may not start with "."')
  }

  return null
}

function email (em) {
  if (!em.match(/^.+@.+\..+$/)) {
    return new Error('Email must be an email address')
  }

  return null
}

function pw (pw) {
  if (pw.match(/['!:@"]/)) {
    return new Error('Sorry, passwords cannot contain these characters: \'!:@"')
  }

  return null
}