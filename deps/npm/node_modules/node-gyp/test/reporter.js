const Mocha = require('mocha')

class Reporter {
  constructor (runner) {
    this.failedTests = []

    runner.on(Mocha.Runner.constants.EVENT_RUN_BEGIN, () => {
      console.log('Starting tests')
    })

    runner.on(Mocha.Runner.constants.EVENT_RUN_END, () => {
      console.log('Tests finished')
      console.log()
      console.log('****************')
      console.log('* TESTS REPORT *')
      console.log('****************')
      console.log()
      console.log(`Executed ${runner.stats.suites} suites with ${runner.stats.tests} tests in ${runner.stats.duration} ms`)
      console.log(`  Passed: ${runner.stats.passes}`)
      console.log(`  Skipped: ${runner.stats.pending}`)
      console.log(`  Failed: ${runner.stats.failures}`)
      if (this.failedTests.length > 0) {
        console.log()
        console.log('  Failed test details')
        this.failedTests.forEach((failedTest, index) => {
          console.log()
          console.log(`    ${index + 1}.'${failedTest.test.fullTitle()}'`)
          console.log(`      Name: ${failedTest.error.name}`)
          console.log(`      Message: ${failedTest.error.message}`)
          console.log(`      Code: ${failedTest.error.code}`)
          console.log(`      Stack: ${failedTest.error.stack}`)
        })
      }
      console.log()
    })

    runner.on(Mocha.Runner.constants.EVENT_SUITE_BEGIN, (suite) => {
      if (suite.root) {
        return
      }
      console.log(`Starting suite '${suite.title}'`)
    })

    runner.on(Mocha.Runner.constants.EVENT_SUITE_END, (suite) => {
      if (suite.root) {
        return
      }
      console.log(`Suite '${suite.title}' finished`)
      console.log()
    })

    runner.on(Mocha.Runner.constants.EVENT_TEST_BEGIN, (test) => {
      console.log(`Starting test '${test.title}'`)
    })

    runner.on(Mocha.Runner.constants.EVENT_TEST_PASS, (test) => {
      console.log(`Test '${test.title}' passed in ${test.duration} ms`)
    })

    runner.on(Mocha.Runner.constants.EVENT_TEST_PENDING, (test) => {
      console.log(`Test '${test.title}' skipped in ${test.duration} ms`)
    })

    runner.on(Mocha.Runner.constants.EVENT_TEST_FAIL, (test, error) => {
      this.failedTests.push({ test, error })
      console.log(`Test '${test.title}' failed in ${test.duration} ms with ${error}`)
    })

    runner.on(Mocha.Runner.constants.EVENT_TEST_END, (test) => {
      console.log()
    })
  }
}

module.exports = Reporter
