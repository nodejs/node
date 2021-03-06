'use strict'

/**
 * Usage:
 *
 * node scripts/update-dist-tags.js --otp <one-time password>
 * node scripts/update-dist-tags.js --otp=<one-time password>
 * node scripts/update-dist-tags.js --otp<one-time password>
 */

const usage = `
Usage:

node scripts/update-dist-tags.js --otp <one-time password>
node scripts/update-dist-tags.js --otp=<one-time password>
node scripts/update-dist-tags.js --otp<one-time password>
`

const { execSync } = require('child_process')
const semver = require('semver')
const path = require('path')

const getMajorVersion = (input) => semver.parse(input).major
const getMinorVersion = (input) => semver.parse(input).minor

// INFO: String templates to generate the tags to update
const LATEST_TAG = (strings, major) => `latest-${major}`
const NEXT_TAG = (strings, major) => `next-${major}`
const TAG_LIST = ['lts', 'next', 'latest']
const REMOVE_TAG = (strings, major, minor) => `v${major}.${minor}-next`

// INFO: Finds `--otp` and subsequently otp value (if present)
const PARSE_OTP_FLAG = new RegExp(/(--otp)(=|\s)?([0-9]{6})?/, 'gm')
// INFO: Used to validate otp value (if not found by other regexp)
const PARSE_OTP_VALUE = new RegExp(/^[0-9]{6}$/, 'g')

const args = process.argv.slice(2)
const versionPath = path.resolve(__dirname, '..', 'package.json')
const { version } = require(versionPath)

// Run Script
main()

function main () {
  const otp = parseOTP(args)
  if (version) {
    const major = getMajorVersion(version)
    const minor = getMinorVersion(version)
    const latestTag = LATEST_TAG`${major}`
    const nextTag = NEXT_TAG`${major}`
    const removeTag = REMOVE_TAG`${major}${minor}`
    const updateList = [].concat(TAG_LIST, latestTag, nextTag)

    updateList.forEach((tag) => {
      setDistTag(tag, version, otp)
    })
    removeDistTag(removeTag, version, otp)
  } else {
    console.error('Invalid semver.')
    process.exit(1)
  }
}

function parseOTP (args) {
  // NOTE: making assumption first _thing_ is a string with "--otp" in it
  const parsedArgs = PARSE_OTP_FLAG.exec(args[0])
  if (!parsedArgs) {
    console.error('Invalid arguments supplied. Must supply --otp flag.')
    console.error(usage)
    process.exit(1)
  }
  // INFO: From the regexp, third group is the OTP code
  const otp = parsedArgs[3]
  switch (args.length) {
    case 0: {
      console.error('No arguments supplied.')
      console.error(usage)
      process.exit(1)
    }
    case 1: {
      // --otp=123456 or --otp123456
      if (otp) {
        return otp
      }
      console.error('Invalid otp value supplied. [CASE 1]')
      process.exit(1)
    }
    case 2: {
      // --otp 123456
      // INFO: validating the second argument is an otp code
      const isValidOtp = PARSE_OTP_VALUE.test(args[1])
      if (isValidOtp) {
        return args[1]
      }
      console.error('Invalid otp value supplied. [CASE 2]')
      process.exit(1)
    }
    default: {
      console.error('Invalid arguments supplied.')
      process.exit(1)
    }
  }
}

function setDistTag (tag, version, otp) {
  try {
    const result = execSync(`npm dist-tag set npm@${version} ${tag} --otp=${otp}`, { encoding: 'utf-8' })
    console.log('Result:', result)
  } catch (err) {
    console.error('Bad dist-tag command.')
    process.exit(1)
  }
}

function removeDistTag (tag, version, otp) {
  try {
    const result = execSync(`npm dist-tag rm npm ${tag} --otp=${otp}`, { encoding: 'utf-8' })
    console.log('Result:', result)
  } catch (err) {
    console.error('Bad dist-tag command.')
    process.exit(1)
  }
}
