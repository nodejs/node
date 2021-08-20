const t = require('tap')
const describeAll = require('../../../../lib/utils/config/describe-all.js')
// this basically ends up being a duplicate of the helpdoc dumped into
// a snapshot, but it verifies that we get the same help output on every
// platform where we run CI.
t.matchSnapshot(describeAll())
