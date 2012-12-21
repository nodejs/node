Supporting modules for GYP testing.

    TestCmd.py
    TestCommon.py

        Modules for generic testing of command-line utilities,
        specifically including the ability to copy a test configuration
        to temporary directories (with default cleanup on exit) as part
        of running test scripts that invoke commands, compare actual
        against expected output, etc.

        Our copies of these come from the SCons project,
        http://www.scons.org/.

    TestGyp.py

        Modules for GYP-specific tests, of course.
