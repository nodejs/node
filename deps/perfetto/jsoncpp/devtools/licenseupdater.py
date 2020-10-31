"""Updates the license text in source file.
"""
from __future__ import print_function

# An existing license is found if the file starts with the string below,
# and ends with the first blank line.
LICENSE_BEGIN = "// Copyright "

BRIEF_LICENSE = LICENSE_BEGIN + """2007-2010 Baptiste Lepilleur and The JsonCpp Authors
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

""".replace('\r\n','\n')

def update_license(path, dry_run, show_diff):
    """Update the license statement in the specified file.
    Parameters:
      path: path of the C++ source file to update.
      dry_run: if True, just print the path of the file that would be updated,
               but don't change it.
      show_diff: if True, print the path of the file that would be modified,
                 as well as the change made to the file. 
    """
    with open(path, 'rt') as fin:
        original_text = fin.read().replace('\r\n','\n')
        newline = fin.newlines and fin.newlines[0] or '\n'
    if not original_text.startswith(LICENSE_BEGIN):
        # No existing license found => prepend it
        new_text = BRIEF_LICENSE + original_text
    else:
        license_end_index = original_text.index('\n\n') # search first blank line
        new_text = BRIEF_LICENSE + original_text[license_end_index+2:]
    if original_text != new_text:
        if not dry_run:
            with open(path, 'wb') as fout:
                fout.write(new_text.replace('\n', newline))
        print('Updated', path)
        if show_diff:
            import difflib
            print('\n'.join(difflib.unified_diff(original_text.split('\n'),
                                                   new_text.split('\n'))))
        return True
    return False

def update_license_in_source_directories(source_dirs, dry_run, show_diff):
    """Updates license text in C++ source files found in directory source_dirs.
    Parameters:
      source_dirs: list of directory to scan for C++ sources. Directories are
                   scanned recursively.
      dry_run: if True, just print the path of the file that would be updated,
               but don't change it.
      show_diff: if True, print the path of the file that would be modified,
                 as well as the change made to the file. 
    """
    from devtools import antglob
    prune_dirs = antglob.prune_dirs + 'scons-local* ./build* ./libs ./dist'
    for source_dir in source_dirs:
        cpp_sources = antglob.glob(source_dir,
            includes = '''**/*.h **/*.cpp **/*.inl''',
            prune_dirs = prune_dirs)
        for source in cpp_sources:
            update_license(source, dry_run, show_diff)

def main():
    usage = """%prog DIR [DIR2...]
Updates license text in sources of the project in source files found
in the directory specified on the command-line.

Example of call:
python devtools\licenseupdater.py include src -n --diff
=> Show change that would be made to the sources.

python devtools\licenseupdater.py include src
=> Update license statement on all sources in directories include/ and src/.
"""
    from optparse import OptionParser
    parser = OptionParser(usage=usage)
    parser.allow_interspersed_args = False
    parser.add_option('-n', '--dry-run', dest="dry_run", action='store_true', default=False,
        help="""Only show what files are updated, do not update the files""")
    parser.add_option('--diff', dest="show_diff", action='store_true', default=False,
        help="""On update, show change made to the file.""")
    parser.enable_interspersed_args()
    options, args = parser.parse_args()
    update_license_in_source_directories(args, options.dry_run, options.show_diff)
    print('Done')

if __name__ == '__main__':
    import sys
    import os.path
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    main()

