//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Import the utility functionality.
import jobs.generation.Utilities;
import jobs.generation.InternalUtilities;

// Grab the github project name passed in
def project = GithubProject

def msbuildTypeMap = [
    'debug':'chk',
    'test':'test',
    'release':'fre'
]

// ----------------
// INNER LOOP TASKS
// ----------------

// Generate the builds for debug and release, commit and PRJob
[true, false].each { isPR -> // Defines a closure over true and false, value assigned to isPR
    ['x86', 'x64', 'arm'].each { buildArch -> // build these architectures
        ['debug', 'test', 'release'].each { buildType -> // build these configurations
            def config = "${buildArch}_${buildType}"

            // Determine the name for the new job.
            // params: Project, BaseTaskName, IsPullRequest (appends _prtest)
            def jobName = InternalUtilities.getFullJobName(project, config, isPR)

            def testableConfig = buildType in ['debug', 'test'] && buildArch != 'arm'
            def analysisConfig = buildType in ['release']

            def buildScript = "call jenkins.buildone.cmd ${buildArch} ${buildType}"
            buildScript += analysisConfig ? ' "/p:runcodeanalysis=true"' : ''
            def testScript = "call jenkins.testone.cmd ${buildArch} ${buildType}"
            def analysisScript = ".\\Build\\scripts\\check_prefast_error.ps1 . CodeAnalysis.err"

            // Create a new job with the specified name.  The brace opens a new closure
            // and calls made within that closure apply to the newly created job.
            def newJob = job(jobName) {
                // This opens the set of build steps that will be run.
                // This looks strange, but it is actually a method call, with a
                // closure as a param, since Groovy allows method calls without parens.
                // (Compare with '.each' method used above.)
                steps {
                    batchFile(buildScript) // run the parameter as a batch script
                    if (testableConfig) {
                        // The test script will only run if the build is successful
                        // because Jenkins will notice the failure and stop before
                        // executing any more build tasks.
                        batchFile(testScript)
                    }
                    if (analysisConfig) {
                        // For release builds we want to run code analysis checks
                        powerShell(analysisScript)
                    }
                }
            }

            Utilities.setMachineAffinity(newJob, 'Windows_NT') // Server 2012 R2

            def msbuildType = msbuildTypeMap.get(buildType)
            def msbuildFlavor = "build_${buildArch}${msbuildType}"

            def archivalString = "test/${msbuildFlavor}.*,test/logs/**"
            archivalString += analysisConfig ? ',CodeAnalysis.err' : ''

            Utilities.addArchival(newJob, archivalString,
                '', // no exclusions from archival
                false, // doNotFailIfNothingArchived=false ~= failIfNothingArchived
                false) // archiveOnlyIfSuccessful=false ~= archiveAlways

            // This call performs remaining common job setup on the newly created job.
            // This is used most commonly for simple inner loop testing.
            // It does the following:
            //   1. Sets up source control for the project.
            //   2. Adds a push trigger if the job is a PR job
            //   3. Adds a github PR trigger if the job is a PR job.
            //      The optional context (label that you see on github in the PR checks) is added.
            //      If not provided the context defaults to the job name.
            //   4. Adds standard options for build retention and timeouts
            //   5. Adds standard parameters for PR and push jobs.
            //      These allow PR jobs to be used for simple private testing, for instance.
            // See the documentation for this function to see additional optional parameters.

            // Note: InternalUtilities variant also sets private permission
            InternalUtilities.simpleInnerLoopJobSetup(newJob, project, isPR, "Windows ${config}")
            // Add private permissions for certain users
            InternalUtilities.addPrivatePermissions(newJob, ['nmostafa', 'arunetm', 'litian2025'])
        }
    }
}

// -----------------
// DAILY BUILD TASKS
// -----------------

// build and test on Windows 7 with VS 2013 (Dev12/MsBuild12)
[true, false]. each { isPR ->
    ['x86', 'x64'].each { buildArch -> // we don't support ARM on Windows 7
        ['debug', 'test', 'release'].each { buildType ->
            def config = "daily_${buildArch}_${buildType}"

            def jobName = InternalUtilities.getFullJobName(project, config, isPR)

            def testableConfig = buildType in ['debug', 'test']
            def analysisConfig = buildType in ['release']

            def buildScript = "call jenkins.buildone.cmd ${buildArch} ${buildType} msbuild12"
            buildScript += analysisConfig ? ' "/p:runcodeanalysis=true"' : ''
            def testScript = "call jenkins.testone.cmd ${buildArch} ${buildType}"
            def analysisScript = ".\\Build\\scripts\\check_prefast_error.ps1 . CodeAnalysis.err"

            def newJob = job(jobName) {
                steps {
                    batchFile(buildScript)
                    if (testableConfig) {
                        batchFile(testScript)
                    }
                    if (analysisConfig) {
                        powerShell(analysisScript)
                    }
                }
            }

            Utilities.setMachineAffinity(newJob, "Windows 7")

            def msbuildType = msbuildTypeMap.get(buildType)
            def msbuildFlavor = "build_${buildArch}${msbuildType}"

            def archivalString = "test/${msbuildFlavor}.*,test/logs/**"
            archivalString += analysisConfig ? ',CodeAnalysis.err' : ''

            Utilities.addArchival(newJob, archivalString,
                '', // no exclusions from archival
                false, // doNotFailIfNothingArchived=false ~= failIfNothingArchived
                false) // archiveOnlyIfSuccessful=false ~= archiveAlways

            InternalUtilities.standardJobSetup(newJob, project, isPR)

            if (isPR) {
                // The addition of the trigger phrase will make the job "non-default" in Github.
                def dailyRegex = '(dailies|dailys?)'
                def groupRegex = '(win(dows)?\\s*7|(dev|msbuild)\\s*12)'
                def triggerName = "Windows 7 ${config}"
                def triggerRegex = "(${dailyRegex}|${groupRegex}|${triggerName})"
                Utilities.addGithubPRTrigger(newJob,
                    triggerName, // GitHub task name
                    "(?i).*test\\W+${triggerRegex}.*")
            } else {
                Utilities.addPeriodicTrigger(newJob, '@daily')
            }

            // Add private permissions for certain users
            InternalUtilities.addPrivatePermissions(newJob, ['nmostafa', 'arunetm', 'litian2025'])
        }
    }
}

// build and test on the usual configuration (VS 2015) with JIT disabled
[true, false]. each { isPR ->
    ['x86', 'x64', 'arm'].each { buildArch ->
        ['debug', 'test', 'release'].each { buildType ->
            def config = "daily_disablejit_${buildArch}_${buildType}"

            def jobName = InternalUtilities.getFullJobName(project, config, isPR)

            def testableConfig = buildType in ['debug', 'test'] && buildArch != 'arm'
            def analysisConfig = buildType in ['release']

            def buildScript = "call jenkins.buildone.cmd ${buildArch} ${buildType}"
            buildScript += ' "/p:BuildJIT=false"' // don't build JIT on this build task
            buildScript += analysisConfig ? ' "/p:runcodeanalysis=true"' : ''
            def testScript = "call jenkins.testone.cmd ${buildArch} ${buildType}"
            testScript += ' -disablejit' // don't run tests which would require JIT to be built
            def analysisScript = ".\\Build\\scripts\\check_prefast_error.ps1 . CodeAnalysis.err"

            def newJob = job(jobName) {
                steps {
                    batchFile(buildScript)
                    if (testableConfig) {
                        batchFile(testScript)
                    }
                    if (analysisConfig) {
                        powerShell(analysisScript)
                    }
                }
            }

            Utilities.setMachineAffinity(newJob, "Windows_NT") // Server 2012 R2

            def msbuildType = msbuildTypeMap.get(buildType)
            def msbuildFlavor = "build_${buildArch}${msbuildType}"

            def archivalString = "test/${msbuildFlavor}.*,test/logs/**"
            archivalString += analysisConfig ? ',CodeAnalysis.err' : ''

            Utilities.addArchival(newJob, archivalString,
                '', // no exclusions from archival
                false, // doNotFailIfNothingArchived=false ~= failIfNothingArchived
                false) // archiveOnlyIfSuccessful=false ~= archiveAlways

            InternalUtilities.standardJobSetup(newJob, project, isPR)

            if (isPR) {
                // The addition of the trigger phrase will make the job "non-default" in Github.
                def dailyRegex = '(dailies|dailys?)'
                def groupRegex = '(disablejit|no(build)?jit|buildjit=false)'
                def triggerName = "Windows ${config}"
                def triggerRegex = "(${dailyRegex}|${groupRegex}|${triggerName})"
                Utilities.addGithubPRTrigger(newJob,
                    triggerName, // GitHub task name
                    "(?i).*test\\W+${triggerRegex}.*")
            } else {
                Utilities.addPeriodicTrigger(newJob, '@daily')
            }

            // Add private permissions for certain users
            InternalUtilities.addPrivatePermissions(newJob, ['nmostafa', 'arunetm', 'litian2025'])
        }
    }
}

// ----------------
// CODE STYLE TASKS
// ----------------

// Create a job to check that no mixed line endings have been introduced.
// Note: it is not necessary to run this job daily.
[true, false].each { isPR -> // Defines a closure over true and false, value assigned to isPR
    def jobName = InternalUtilities.getFullJobName(project, 'ubuntu_check_eol', isPR)

    def taskString = './jenkins.check_eol.sh'
    def newJob = job(jobName) {
        label('ubuntu')
        steps {
            shell(taskString)
        }
    }

    // Note: InternalUtilities variant also sets private permission
    InternalUtilities.simpleInnerLoopJobSetup(newJob, project, isPR, "EOL Check")
    // Add private permissions for certain users
    InternalUtilities.addPrivatePermissions(newJob, ['nmostafa', 'arunetm', 'litian2025'])
}

// Create a job to check that all source files have the correct Microsoft copyright notice.
// Note: it is not necessary to run this job daily.
[true, false].each { isPR -> // Defines a closure over true and false, value assigned to isPR
    def jobName = InternalUtilities.getFullJobName(project, 'ubuntu_check_copyright', isPR)

    def taskString = './jenkins.check_copyright.sh'
    def newJob = job(jobName) {
        label('ubuntu')
        steps {
            shell(taskString)
        }
    }

    // Note: InternalUtilities variant also sets private permission
    InternalUtilities.simpleInnerLoopJobSetup(newJob, project, isPR, "Copyright Check")
    // Add private permissions for certain users
    InternalUtilities.addPrivatePermissions(newJob, ['nmostafa', 'arunetm', 'litian2025'])
}
