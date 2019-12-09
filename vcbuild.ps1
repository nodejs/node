if ($args[0] -eq 'help') {
    Write-Output @"
vcbuild.ps1 [debug/release] [msi] [doc] [test/test-all/test-addons/test-js-native-api/test-node-api/test-benchmark/test-internet/test-pummel/test-simple/test-message/test-tick-processor/test-known-issues/test-node-inspect/test-check-deopts/test-npm/test-async-hooks/test-v8/test-v8-intl/test-v8-benchmarks/test-v8-all] [ignore-flaky] [static/dll] [noprojgen] [projgen] [small-icu/full-icu/without-intl] [nobuild] [nosnapshot] [noetw] [ltcg] [licensetf] [sign] [ia32/x86/x64/arm64] [vs2017] [download-all] [enable-vtune] [lint/lint-ci/lint-js/lint-js-ci/lint-md] [lint-md-build] [package] [build-release] [upload] [no-NODE-OPTIONS] [link-module path-to-module] [debug-http2] [debug-nghttp2] [clean] [cctest] [no-cctest] [openssl-no-asm]
Examples:
  vcbuild.ps1                          : builds release build
  vcbuild.ps1 debug                    : builds debug build
  vcbuild.ps1 release msi              : builds release build and MSI installer package
  vcbuild.ps1 test                     : builds debug build and runs tests
  vcbuild.ps1 build-release            : builds the release distribution as used by nodejs.org
  vcbuild.ps1 enable-vtune             : builds nodejs with Intel VTune profiling support to profile JavaScript
  vcbuild.ps1 link-module my_module.js : bundles my_module as built-in module
  vcbuild.ps1 lint                     : runs the C++, documentation and JavaScript linter
  vcbuild.ps1 no-cctest                : skip building cctest.exe
"@
    exit
}

Set-Location $PSScriptRoot

# CI_* variables should be kept synchronized with the ones in Makefile
$CI_NATIVE_SUITES = 'addons js-native-api node-api'
$CI_JS_SUITES = 'default'
$CI_DOC = 'doctool'
# Same as the test-ci target in Makefile
$common_test_suites = "$CI_JS_SUITES $CI_NATIVE_SUITES $CI_DOC"
$build_addons = 1
$build_js_native_api_tests = 1
$build_node_api_tests = 1

# Process arguments.
$config = 'Release'
$target = 'Build'
$target_arch = 'x64'

$configure_flags = New-Object System.Collections.ArrayList
$test_args = New-Object System.Collections.ArrayList

switch ($args[0]) {
    "debug" { $config = 'Debug' }
    "release" { 
        $configure_flags.Add('--with-ltcg')
        $cctest = 1 
    }
    "clean" { $target = 'Clean' }
    "testclean" { $target = 'TestClean' }
    "ia32" { $_ = 'x86' }
    "x86" { $target_arch = 'x86' }
    "x64" { $target_arch = 'x64' }
    "arm64" { $target_arch = 'arm64' }
    "vs2017" { $target_env = 'vs2017' }
    "vs2019" { $target_env = 'vs2019' }
    "noprojgen" { $noprojgen = 1 }
    "projgen" { $projgen = 1 }
    "nobuild" { $nobuild = 1 }
    "nosign" {
        Write-Output 'Note: vcbuild no longer signs by default. "nosign" is redundant.'
    }
    "sign" { $sign = 1 }
    "nosnapshot" { $configure_flags.Add('--without-snapshot') }
    "noetw" { $configure_flags.Add('--without-etw') } # noetw_msi_arg
    "ltcg" { $configure_flags.Add('--with-ltcg') }
    "licensertf" { }
    "test" {
        $test_args.Add("-J $common_test_suites") 
        $lint_cpp = 1
        $lint_js = 1
        $lint_md = 1
    }
    "test-ci-native" {
        $test_args.Add($env:test_ci_args)
        $build_addons = 1
    }
    "test-ci-js" {
        $test_args.Add($env:test_ci_args) 
    }
    "build-addons" { $build_addons = 1 }
    "build-js-native-api-tests" { }
    "build-node-api-tests" { $build_node_api_tests = 1 }
    "test-addons" { 
        $test_args.Add('addons')
        $build_addons = 1 
    }
    "test-js-native-api" { 
        $test_args.Add('js-native-api')
        $build_js_native_api_tests = 1
    }
    "test-node-api" {
        $test_args.Add('node-api')
        $build_node_api_tests = 1
    }
    "test-benchmark" { 
        $test_args.Add('benchmark')
    }
    "test-simple" { $test_args.Add('sequential parallel -J') }
    "test-message" { $test_args.Add('message') }
    "test-tick-processor" { $test_args.Add('tick-processor') }
    "test-internet" { $test_args.Add('internet') }
    "test-pummel" { $test_args.Add('pummel') }
    "test-known-issues" { $test_args.Add('known_issues') }
    "test-async-hooks" { $test_args.Add('async-hooks') }
    "test-all" {
        $test_args.Add("gc internet pummel $common_test_suites") 
        $lint_cpp = 1
        $lint_js = 1
    }
    "test-node-inspect" { $test_node_inspect = 1 }
    "test-check-deopts" { $test_check_deopts = 1 }
    "test-npm" { $test_npm = 1 }
    "test-v8" { 
        $test_v8 = 1
        $custom_v8_test = 1
    }
    "test-v8-intl" { 
        $test_v8_intl = 1
        $custom_v8_test = 1
    }
    "test-v8-benchmarks" {
        $test_v8_benchmarks = 1
        $custom_v8_test = 1
    }
    "test-v8-all" {
        $test_v8 = 1 
        $test_v8_intl = 1
        $test_v8_benchmarks = 1
        $custom_v8_test = 1
    }
    "jslint" {
        Write-Output 'Please use lint-js instead of jslint'
        $_ = 'lint-js'
    }
    "jslint-ci" {
        Write-Output 'Please use lint-js-ci instead of jslint-ci'
        $_ = 'lint-js-ci'
    }
    "lint-cpp" { $lint_cpp = 1 }
    "lint-js" { $lint_js = 1 }
    "lint-js-ci" { $lint_js_ci = 1 }
    "lint-md" { $lint_md = 1 }
    "lint-md-build" { $lint_md_build = 1 }
    "lint" { 
        $lint_cpp = 1
        $lint_js = 1
        $lint_md = 1
    }
    "lint-ci" {
        $lint_cpp = 1
        $lint_js_ci = 1   
    }
    "package" { $package = 1 }
    "msi" { 
        $msi = 1
        $licensertf = 1
        $configure_flags.Add("--download=all")
        $configure_flags.Add('--with-intl=full-icu')
    }
    "build-release" {
        $package = 1
        $msi = 1
        $licensertf = 1
        $configure_flags.Add("--download=all")
        $configure_flags.Add('--with-intl=full-icu')
        $projgen = 1
        $cctest = 1
        $configure_flags.Add('--with-ltcg')
        $sign = 1
    }
    "upload" { $upload = 1 }
    "small-icu" { $configure_flags.Add('--with-intl=small-icu') }
    "full-icu" { $configure_flags.Add('--with-intl=full-icu') }
    "intl-none" { $configure_flags.Add('--with-intl=none') }
    "without-intl" { $configure_flags.Add('--with-intl=none') }
    "download-all" { $configure_flags.Add("--download=all") }
    "ignore-flaky" { $test_args.Add('--flaky-tests=dontcare') }
    "enable-vtune" { $configure_flags.Add('--enable-vtune-profiling') }
    "dll" { $configure_flags.Add('--shared') }
    "static" { $configure_flags.Add('--enable-static') }
    "no-NODE-OPTIONS" { $configure_flags.Add('--without-node-options') }
    "debug-nghttp2" { }
    "link-module" { $configure_flags.Add("--link-module $($args[1])") }
    "no-cctest" { $no_cctest = 1 }
    "cctest" { $cctest = 1 }
    "openssl-no-asm" {
        $openssl_no_asm = 1
        $configure_flags.Add('--openssl-no-asm') 
    }
    "doc" { $doc = 1 }
    "binlog" { $extra_msbuild_args = "/binaryLogger:$config\node.binlog" }
    default {
        Write-Output "Error: invalid command line option ``$_``"
        exit 1
    }
}

$test_args = $test_args.ToString()

# assign path to node_exe
$node_exe = "$config\node.exe"
$node_gyp_exe = "$node_exe deps\npm\node_modules\node-gyp\bin\node-gyp"
$npm_exe = "$node_exe deps\npm\bin\npm-cli.js"
if ($target_env) {
    $node_gyp_exe = "$node_gyp_exe --msvs_version=$target_env"
}

# skip building if the only argument received was lint
if ($args[0] -contains 'lint') {
    Invoke-Lint
    exit
}

if (Test-Path 'deps\icu' -and $target -eq 'Clean') {
    Write-Output 'deleting %~dp0deps\icu'
    Remove-Item deps\icu
}

if ($target -eq 'TestClean') {
    Write-Output 'deleting test/.tmp*'
    Remove-Item test\.tmp*
}

& tools\msvs\find_python.cmd
if ($lastexitcode) { exit $lastexitcode }

# NASM is only needed on IA32 and x86_64.
if ((-not $openssl_no_asm) -and ($target_arch -ne 'arm64')) {
    & tools\msvs\find_nasm.cmd
    if ($lastexitcode) {
        Write-Output 'Could not find NASM, install it or build with openssl-no-asm. See BUILDING.md.'
    }
}

Get-Node-Version

if ($TAG) { $configure_flags.Add("--tag=$TAG") }

if ($target -eq 'Clean') {
    Remove-Item $config$TARGET_NAME
}

if (-not $noprojgen) {
    # Set environment for msbuild
    $msvs_host_arch = 'x86'
    if ($env:PROCESSOR_ARCHITECTURE -eq 'AMD64' -or $env:PROCESSOR_ARCHITEW6432 -eq 'AMD64') {
        $msvs_host_arch = 'amd64'
    }
    # usually vcvarsall takes an argument: host + '_' + target
    $vcvarsall_arg = "$msvs_host_arch\_$target_arch"
    # unless both host and target are x64
    if ($target_arch -eq 'x64' -and $msvs_host_arch -eq 'amd64') {
        $vcvarsall_arg = 'amd64'
    }
    # also if both are x86
    if ($target_arch -eq 'x86' -and $msvs_host_arch -eq 'x86') {
        $vcvarsall_arg = 'x86'
    }

    switch ($target_env) {
        $null { $_ = 'vs2019' }
        'vs2019' {
            Write-Output 'Looking for Visual Studio 2019'
            # VCINSTALLDIR may be set if run from a VS Command Prompt and needs to be
            # cleared first as vswhere_usability_wrapper.cmd doesn't when it fails to
            # detect the version searched for
            # TODO: set env here
            & tools\msvs\vswhere_usability_wrapper.cmd "[16.0,17.0)"
            
            $_ = 'vs2017'
        }
        'vs2017' {
            Invoke-msbuild-not-found
        }
    }
}

# Skip build if requested.
if ($nobuild) {
    $msbcpu = '/m:2'
    if ($NUMBER_OF_PROCESSORS -eq '1') { $msbcpu = '/m:1' }
    $msbplatform = 'Win32'
    if ($target_arch -eq 'x64') { $msbplatform = 'x64' }
    if ($target_arch -eq 'arm64') { $msbplatform = 'ARM64' }
    if ($target -eq 'build') {
        if (-not $no_cctest -or -not $test_args -eq '') { $target = 'node' }
        if ($cctest) { $target = 'Build' }
    }
    if ($target -eq 'node') {
        Remove-Item "$config\cctest.exe"
    }
    $msbuild_args = $env:msbuild_args
    if ($msbuild_args) {
        $extra_msbuild_args = "$extra_msbuild_args $msbuild_args"
    }
    & msbuild node.sln $msbcpu /t:$target /p:Configuration=$config /p:Platform=$msbplatform /clp:NoItemAndPropertyList; Verbosity=minimal /nologo $extra_msbuild_args
    if ($lastexitcode) {
        if (-not $project_generated) {
            Write-Output 'Building Node with reused solution failed. To regenerate project files use "vcbuild projgen"'
        }
        exit 1
    }
    if ($target -eq 'Clean') { exit }
}

function Invoke-msbuild-not-found {
    Write-Output 'Failed to find a suitable Visual Studio installation.'
    Write-Output 'Try to run in a "Developer Command Prompt" or consult'
    Write-Output 'https://github.com/nodejs/node/blob/master/BUILDING.md#windows-1'
    exit
}

cmd /c rd $config
if ($lastexitcode) {
    Write-Output  "Old build output exists at 'out\%config%'. Please remove."
    exit
}
if (Test-Path "out\$config") {
    # Use /J because /D (symlink) requires special permissions.
    cmd /c mklink /J $config out\$config
    if ($lastexitcode) {
        Write-Output "Could not create junction to 'out\%config%'."
        exit
    }
}

# Skip signing unless the `sign` option was specified.
if ($sign) {
    & tools\sign.bat Release\node.exe
    if ($lastexitcode) { exit $lastexitcode }
}

# Skip license.rtf generation if not requested.
if ($licensertf) {
    & $node_exe tools\license2rtf.js | LICENSE > $config\license.rtf
    if ($lastexitcode) {
        Write-Output Failed to generate license.rtf
        exit $lastexitcode 
    }
}

if ($msi -or $package) {
    Write-Output 'Creating package...'
    Set-Location Release
    # Remove-Item 

    Set-Location ..
    
    if ($package) {
        Set-Location Release
        Write-Output "Creating $TARGET_NAME.7z"
        Remove-Item "$TARGET_NAME.7z"
        & 7z a -r -mx9 -t7z "$TARGET_NAME.7z" $TARGET_NAME
        if ($lastexitcode) { Invoke-Package-Error }
    }

    # Skip msi generation if not requested
    if ($msi) {
        Write-Output "echo Building node-v$FULLVERSION-$target_arch.msi"
        if ($env:WindowsSDKVersion) {
            # $msbsdk=
        }
        & msbuild "%~dp0tools\msvs\msi\nodemsi.sln" /m /t:Clean, Build $msbsdk /p:PlatformToolset=%PLATFORM_TOOLSET% /p:GypMsvsVersion=%GYP_MSVS_VERSION% /p:Configuration=%config% /p:Platform=%target_arch% /p:NodeVersion=%NODE_VERSION% /p:FullVersion=%FULLVERSION% /p:DistTypeDir=%DISTTYPEDIR% %noetw_msi_arg% /clp:NoSummary; NoItemAndPropertyList; Verbosity=minimal /nologo
        if ($lastexitcode) {
            Write-Output 'Failed to sign msi'
            exit $lastexitcode
        }
    }
}

if ($sign) {
    & call tools\sign.bat node-v$FULLVERSION-$target_arch.msi
    if ($lastexitcode) {
        Write-Output 'Failed to sign msi'
        exit $lastexitcode
    }
}

if ($upload) {
    $SSHCONFIG = $env:SSHCONFIG
    if (!$SSHCONFIG) {
        Write-Output 'SSHCONFIG is not set for upload'
        exit 1
    }
    $STAGINGSERVER = $env:STAGINGSERVER -or 'node-www'
    & ssh -F $SSHCONFIG $STAGINGSERVER "mkdir -p nodejs/%DISTTYPEDIR%/v%FULLVERSION%/win-%target_arch%"
    if ($lastexitcode) { exit $lastexitcode }
    #
}

# Run tests if requested.
if ($build_addons) {
    if (Test-Path $node_exe) {
        Write-Output 'Building addons'
        #
    }
    else {
        Write-Output 'Failed to find node.exe'
    }
}

if ($build_js_native_api_tests) {
    if (Test-Path $node_exe) {
        Write-Output 'Building js-native-api'
        #
    }
    else {
        Write-Output 'Failed to find node.exe'
    }
}

if ($test_check_deopts) {
    & python tools\test.py --mode=release --check-deopts parallel sequential -J 
}

if ($test_node_inspect) {
    $env:USE_EMBEDDED_NODE_INSPECT = 1
    & $node_exe tools\test-npm-package.js --install deps\node-inspect test
}

if ($test_npm) {
    $npm_test_cmd = "$node_exe tools\test-npm-package.js --install --logfile=test-npm.tap deps\npm test-node"
    Write-Output $npm_test_cmd
    & $npm_test_cmd
}

function Invoke-Package-Error {
    Set-Location ..
    exit 1
}

function Invoke-Upload {
    
}
function Invoke-Lint {
    if ($lint_cpp) {
        if (!$env:NODEJS_MAKE) {
            # TODO
        }
        Write-Output 'Could not find GNU Make, needed for linting C/C++'
        & $env:NODEJS_MAKE lint-cpp
    }

    if (-not (Test-Path tools\node_modules\eslint)) {
        Write-Output 'Linting is not available through the source tarball.'
        Write-Output 'Use the git repo instead: $ git clone https://github.com/nodejs/node.git'
    }

    if ($lint_js_ci) {
        Write-Output 'running lint-js-ci'
        & $node_exe tools\lint-js.js -J -f tap -o test-eslint.tap benchmark doc lib test tools
    }
}

function Invoke-Lint-JS {
    if (!(Test-Path tools\node_modules\eslint)) {
        return
    }
    & $node_exe tools\node_modules\eslint\bin\eslint.js --cache --report-unused-disable-directives --rule "linebreak-style: 0" --ext=.js, .mjs, .md .eslintrc.js benchmark doc lib test tools
}

function Invoke-Lint-JS-CI {
    & $node_exe tools\lint-js.js -J -f tap -o test-eslint.tap benchmark doc lib test tools
}
function Get-Node-Version {
    $NODE_VERSION = python .\tools\getnodeversion.py | Out-String
    if (-not $NODE_VERSION) {
        Write-Output 'Cannot determine current version of Node.js'
        exit 1
    }

    $DISTTYPE = $env:DISTTYPE -or 'release'
    switch ($DISTTYPE) {
        'release' { $FULLVERSION = $NODE_VERSION }
        'custom' {
            if (-not $env:CUSTOMTAG) {
                Write-Output "CUSTOMTAG is not set for DISTTYPE=custom"
                exit 1
            }
            $TAG = $env:CUSTOMTAG
            $FULLVERSION = "$NODE_VERSION-$TAG"
        }
        default {
            if (-not $env:DATESTRING) {
                Write-Output "DATESTRING is not set for nightly"
                exit 1
            }
            if (-not $env:COMMIT) {
                Write-Output "COMMIT is not set for nightly"
                exit 1
            }
            if ($DISTTYPE -ne 'nightly' -and $DISTTYPE -ne 'next-nightly') {
                Write-Output "DISTTYPE is not release, custom, nightly or next-nightly"
                exit 1
            }
            $TAG = "$DISTTYPE$DATESTRING$COMMIT"
            $FULLVERSION = "$NODE_VERSION-$TAG"
        }
    }
}
