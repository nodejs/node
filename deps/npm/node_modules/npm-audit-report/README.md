# npm audit security report

Given a response from the npm security api, render it into a variety of security reports

The response is an object that contains an output string (the report) and a suggested exitCode.
```
{
  report: 'string that contains the security report',
  exit: 1
}
```


## Basic usage example

This is intended to be used along with
[`@npmcli/arborist`](http://npm.im/@npmcli/arborist)'s `AuditReport` class.

```
'use strict'
const Report = require('npm-audit-report')
const options = {
  reporter: 'json'
}

const arb = new Arborist({ path: '/path/to/project' })
arb.audit().then(report => {
  const result = new Report(report, options)
  console.log(result.output)
  process.exitCode = result.exitCode
})
```

## Break from Version 1

Version 5 and 6 of the npm CLI make a request to the registry endpoint at
either the "Full Audit" endpoint at `/-/npm/v1/security/audits` or
the "Quick Audit" endpoint at `/-/npm/v1/security/audits/quick`.  The Full
Audit endpoint calculates remediations necessary to correct problems based
on the shape of the tree.

As of npm v7, the logic of how the cli manages trees is dramatically
rearchitected, rendering much of the remediations no longer valid.
Thus, it _only_ fetches the advisory data from the Quick Audit endpoint,
and uses [`@npmcli/arborist`](http://npm.im/@npmcli/arborist) to calculate
required remediations and affected nodes in the dependency graph.  This
data is serialized and provided as an `"auditReportVersion": 2` object.

Version 2 of this module expects to recieve an instance (or serialized JSON
version of) the `AuditReport` class from Arborist, which is returned by
`arborist.audit()` and stored on the instance as `arborist.auditReport`.

Eventually, a new endpoint _may_ be added to move the `@npmcli/arborist` work
to the server-side, in which case version 2 style audit reports _may_ be
provided directly.

## options

| option   | values                               | default   | description |
| :---     | :---                                 | :---      |:--- |
| reporter | `install`, `detail`, `json`, `quiet` | `install` | specify which output format you want to use |
| color    | `true`, `false`                      | `true`    | indicates if some report elements should use colors |
| unicode  | `true`, `false`                      | `true`    | indicates if unicode characters should be used|
| indent   | Number or String                     | `2`       | indentation for `'json'` report|
| auditLevel | 'info', 'low', 'moderate', 'high', 'critical', 'none' | `low` (ie, exit 0 if only `info` advisories are found) | level of vulnerability that will trigger a non-zero exit code (set to 'none' to always exit with a 0 status code) |
