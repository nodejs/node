'use strict'
/*
  this allows platform dependent mapping of flags that relies on the result
  of process.platform and process.arch

  logic is:
  platform > compiler | linker | others > key of content
 */
module.exports = {
  darwin: {
    compiler: {
      arch: (platform) => {
        return {
          arm: ['-arch', 'arm'], //TODO: this is likely not valid; doc is missing on AAPL gcc
          ia32: ['-arch', 'i386'],
          x64: ['-arch', 'x86_64']
        }[platform]
      }
    },
    linker: {
      arch: (platform) => {
        return {
          arm: ['-arch', 'arm'], //TODO: this is likely not valid; doc is missing on AAPL gcc
          ia32: ['-arch', 'i386'],
          x64: ['-arch', 'x86_64']
        }[platform]
      }
    },
    osx_min_version: (version) => {
      return [`-mmacosx-version-min=${version}`]
    }
  },
  freebsd: {
    compiler: {

    },
    linker: {

    }
  },
  linux: {
    compiler: {
      arch: (platform) => {
        return {
          arm: ['-marm'], // REVIEW
          ia32: ['-m32'],
          x64: ['-m64']
        }[platform]
      }
    },
    linker: {
      arch: (platform) => {
        return {
          arm: ['-marm'], // REVIEW
          ia32: ['-m32'],
          x64: ['-m64']
        }[platform]
      }
    }
  },
  sunos: {
    compiler: {

    },
    linker: {

    }
  },
  win32: {
    compiler: {

    },
    linker: {

    }
  }
}
