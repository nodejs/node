#!/usr/bin/env python
# encoding: utf-8
# Yinon dot me gmail 2008

"""
these constants are somewhat public, try not to mess them

maintainer: the version number is updated from the top-level wscript file
"""

# do not touch these three lines, they are updated automatically
HEXVERSION=0x105016
WAFVERSION="1.5.16"
WAFREVISION = "7610:7647M"
ABI = 7

# permissions
O644 = 420
O755 = 493

MAXJOBS = 99999999

CACHE_DIR          = 'c4che'
CACHE_SUFFIX       = '.cache.py'
DBFILE             = '.wafpickle-%d' % ABI
WSCRIPT_FILE       = 'wscript'
WSCRIPT_BUILD_FILE = 'wscript_build'
WAF_CONFIG_LOG     = 'config.log'
WAF_CONFIG_H       = 'config.h'

SIG_NIL = 'iluvcuteoverload'

VARIANT = '_VARIANT_'
DEFAULT = 'default'

SRCDIR  = 'srcdir'
BLDDIR  = 'blddir'
APPNAME = 'APPNAME'
VERSION = 'VERSION'

DEFINES = 'defines'
UNDEFINED = ()

BREAK = "break"
CONTINUE = "continue"

# task scheduler options
JOBCONTROL = "JOBCONTROL"
MAXPARALLEL = "MAXPARALLEL"
NORMAL = "NORMAL"

# task state
NOT_RUN = 0
MISSING = 1
CRASHED = 2
EXCEPTION = 3
SKIPPED = 8
SUCCESS = 9

ASK_LATER = -1
SKIP_ME = -2
RUN_ME = -3


LOG_FORMAT = "%(asctime)s %(c1)s%(zone)s%(c2)s %(message)s"
HOUR_FORMAT = "%H:%M:%S"

TEST_OK = True

CFG_FILES = 'cfg_files'

# positive '->' install
# negative '<-' uninstall
INSTALL = 1337
UNINSTALL = -1337

