Supporting modules for GYP testing.

    SConsLib/

        Modules for generic testing of command-line utilities,
        specifically including the ability to copy a test configuration
        to temporary directories (with default cleanup on exit) as part
        of running test scripts that invoke commands, compare actual
        against expected output, etc.

        Our copies of these come from the SCons project:
        https://github.com/SCons/scons/tree/887f4a1b06ceed33ee6ba4c5589a32a607d6b001/testing/framework

    TestGyp.py

        Modules for GYP-specific tests, of course ;)
