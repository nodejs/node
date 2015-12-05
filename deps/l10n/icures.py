#!/usr/bin/python

import sys
import shutil
reload(sys)
sys.setdefaultencoding("utf-8")

import optparse
import os
import glob

endian=sys.byteorder

parser = optparse.OptionParser(usage="usage: %prog -n {NAME} -d {DEST} -i {ICU}")

parser.add_option("-d", "--dest-dir",
                  action="store",
                  dest="dest",
                  help="The destination directory")

parser.add_option("-n", "--name",
                  action="store",
                  dest="name",
                  help="The application package name")

parser.add_option("-i", "--icu-path",
                  action="store",
                  dest="icu",
                  help="The ICU tool path")

parser.add_option("-l", "--endian",
                  action="store",
                  dest="endian",
                  help='endian: big, little or host. your default is "%s"' % endian, default=endian, metavar='endianess')

(options, args) = parser.parse_args()

optVars = vars(options);

for opt in ["dest", "name", "icu"]:
    if optVars[opt] is None:
        parser.error("Missing required option: %s" % opt)
        sys.exit(1)

if options.endian not in ("big", "little", "host"):
    parser.error("Unknown endianess: %s" % options.endian)
    sys.exit(1)

if options.endian == "host":
    options.endian = endian

if not os.path.isdir(options.dest):
    parser.error("Destination is not a directory")
    sys.exit(1)

if options.icu[-1] is '"':
    options.icu = options.icu[:-1]

if not os.path.isdir(options.icu):
    parser.error("ICU Path is not a directory")
    sys.exit(1)

if options.icu[-1] != os.path.sep:
    options.icu += os.path.sep

genrb = options.icu + 'genrb'
icupkg = options.icu + 'icupkg'

if sys.platform.startswith('win32'):
    genrb += '.exe'
    icupkg += '.exe'

if not os.path.isfile(genrb):
    parser.error('ICU Tool "%s" does not exist' % genrb)
    sys.exit(1)

if not os.path.isfile(icupkg):
    parser.error('ICU Tool "%s" does not exist' % icupkg)
    sys.exit(1)

def runcmd(tool, cmd, doContinue=False):
    cmd = "%s %s" % (tool, cmd)
    rc = os.system(cmd)
    if rc is not 0 and not doContinue:
        print "FAILED: %s" % cmd
        sys.exit(1)
    return rc

resfiles = glob.glob("%s%s*.res" % (options.dest, os.path.sep))
_listfile = os.path.join(options.dest, 'packagefile.lst')
datfile = "%s%s%s.dat" % (options.dest, os.path.sep, options.name)

def clean():
    for f in resfiles:
        if os.path.isfile(f):
            os.remove(f)
    if os.path.isfile(_listfile):
        os.remove(_listfile)
    if (os.path.isfile(datfile)):
        os.remove(datfile)

## Step 0, Clean up from previous build
clean()

## Step 1, compile the txt files in res files

if sys.platform.startswith('win32'):
  srcfiles = glob.glob('resources/*.txt')
  runcmd(genrb, "-e utf16 -d %s %s" % (options.dest, " ".join(srcfiles)))
else:
  runcmd(genrb, "-e utf16 -d %s resources%s*.txt" % (options.dest, os.path.sep))

resfiles = [os.path.relpath(f) for f in glob.glob("%s%s*.res" % (options.dest, os.path.sep))]

# pkgdata requires relative paths... it's annoying but necessary
# for us to change into the dest directory to work
cwd = os.getcwd();
os.chdir(options.dest)

## Step 2, generate the package list
listfile = open(_listfile, 'w')
listfile.write(" ".join([os.path.basename(f) for f in resfiles]))
listfile.close()

## Step 3, generate the dat file using icupkg and the package list
runcmd(icupkg, '-a packagefile.lst new %s.dat' % options.name);

## All done with this tool at this point...
os.chdir(cwd); # go back to original working directory
