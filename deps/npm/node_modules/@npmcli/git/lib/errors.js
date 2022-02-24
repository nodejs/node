
const maxRetry = 3

class GitError extends Error {
  shouldRetry () {
    return false
  }
}

class GitConnectionError extends GitError {
  constructor (message) {
    super('A git connection error occurred')
  }

  shouldRetry (number) {
    return number < maxRetry
  }
}

class GitPathspecError extends GitError {
  constructor (message) {
    super('The git reference could not be found')
  }
}

class GitUnknownError extends GitError {
  constructor (message) {
    super('An unknown git error occurred')
  }
}

module.exports = {
  GitConnectionError,
  GitPathspecError,
  GitUnknownError,
}
