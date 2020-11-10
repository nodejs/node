# npm audit security report

Given a response from the npm security api, render it into a variety of security reports

[![Build Status](https://travis-ci.org/npm/npm-audit-report.svg?branch=master)](https://travis-ci.org/npm/npm-audit-report)
[![Build status](https://ci.appveyor.com/api/projects/status/qictiokvxmqkiuvi/branch/master?svg=true)](https://ci.appveyor.com/project/evilpacket/npm-audit-report/branch/master)
[![Coverage Status](https://coveralls.io/repos/github/npm/npm-audit-report/badge.svg?branch=master)](https://coveralls.io/github/npm/npm-audit-report?branch=master)

The response is an object that contains an output string (the report) and a suggested exitCode.
```
{
  report: 'string that contains the security report',
  exit: 1
}
```


## Basic usage example

```
'use strict'
const Report = require('npm-audit-report')
const options = {
  reporter: 'json'
}

Report(response, options, (result) => {
  console.log(result.report)
  process.exitCode = result.exitCode
})
```


## options

| option        | values                               | default   | description |
| :---          | :---                                 | :---      |:--- |
| reporter      | `install`, `detail`, `json`, `quiet` | `install` | specify which output format you want to use |
| withColor     | `true`, `false`                      | `true`    | indicates if some report elements should use colors |
| withUnicode   | `true`, `false`                      | `true`    | indicates if unicode characters should be used|