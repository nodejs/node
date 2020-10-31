"""Script to generate doxygen documentation.
"""
from __future__ import print_function
from __future__ import unicode_literals
from devtools import tarball
from contextlib import contextmanager
import subprocess
import traceback
import re
import os
import sys
import shutil

@contextmanager
def cd(newdir):
    """
    http://stackoverflow.com/questions/431684/how-do-i-cd-in-python
    """
    prevdir = os.getcwd()
    os.chdir(newdir)
    try:
        yield
    finally:
        os.chdir(prevdir)

def find_program(*filenames):
    """find a program in folders path_lst, and sets env[var]
    @param filenames: a list of possible names of the program to search for
    @return: the full path of the filename if found, or '' if filename could not be found
"""
    paths = os.environ.get('PATH', '').split(os.pathsep)
    suffixes = ('win32' in sys.platform) and '.exe .com .bat .cmd' or ''
    for filename in filenames:
        for name in [filename+ext for ext in suffixes.split(' ')]:
            for directory in paths:
                full_path = os.path.join(directory, name)
                if os.path.isfile(full_path):
                    return full_path
    return ''

def do_subst_in_file(targetfile, sourcefile, dict):
    """Replace all instances of the keys of dict with their values.
    For example, if dict is {'%VERSION%': '1.2345', '%BASE%': 'MyProg'},
    then all instances of %VERSION% in the file will be replaced with 1.2345 etc.
    """
    with open(sourcefile, 'r') as f:
        contents = f.read()
    for (k,v) in list(dict.items()):
        v = v.replace('\\','\\\\') 
        contents = re.sub(k, v, contents)
    with open(targetfile, 'w') as f:
        f.write(contents)

def getstatusoutput(cmd):
    """cmd is a list.
    """
    try:
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output, _ = process.communicate()
        status = process.returncode
    except:
        status = -1
        output = traceback.format_exc()
    return status, output

def run_cmd(cmd, silent=False):
    """Raise exception on failure.
    """
    info = 'Running: %r in %r' %(' '.join(cmd), os.getcwd())
    print(info)
    sys.stdout.flush()
    if silent:
        status, output = getstatusoutput(cmd)
    else:
        status, output = subprocess.call(cmd), ''
    if status:
        msg = 'Error while %s ...\n\terror=%d, output="""%s"""' %(info, status, output)
        raise Exception(msg)

def assert_is_exe(path):
    if not path:
        raise Exception('path is empty.')
    if not os.path.isfile(path):
        raise Exception('%r is not a file.' %path)
    if not os.access(path, os.X_OK):
        raise Exception('%r is not executable by this user.' %path)

def run_doxygen(doxygen_path, config_file, working_dir, is_silent):
    assert_is_exe(doxygen_path)
    config_file = os.path.abspath(config_file)
    with cd(working_dir):
        cmd = [doxygen_path, config_file]
        run_cmd(cmd, is_silent)

def build_doc(options,  make_release=False):
    if make_release:
        options.make_tarball = True
        options.with_dot = True
        options.with_html_help = True
        options.with_uml_look = True
        options.open = False
        options.silent = True

    version = open('version', 'rt').read().strip()
    output_dir = 'dist/doxygen' # relative to doc/doxyfile location.
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    top_dir = os.path.abspath('.')
    html_output_dirname = 'jsoncpp-api-html-' + version
    tarball_path = os.path.join('dist', html_output_dirname + '.tar.gz')
    warning_log_path = os.path.join(output_dir, '../jsoncpp-doxygen-warning.log')
    html_output_path = os.path.join(output_dir, html_output_dirname)
    def yesno(bool):
        return bool and 'YES' or 'NO'
    subst_keys = {
        '%JSONCPP_VERSION%': version,
        '%DOC_TOPDIR%': '',
        '%TOPDIR%': top_dir,
        '%HTML_OUTPUT%': os.path.join('..', output_dir, html_output_dirname),
        '%HAVE_DOT%': yesno(options.with_dot),
        '%DOT_PATH%': os.path.split(options.dot_path)[0],
        '%HTML_HELP%': yesno(options.with_html_help),
        '%UML_LOOK%': yesno(options.with_uml_look),
        '%WARNING_LOG_PATH%': os.path.join('..', warning_log_path)
        }

    if os.path.isdir(output_dir):
        print('Deleting directory:', output_dir)
        shutil.rmtree(output_dir)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    do_subst_in_file('doc/doxyfile', options.doxyfile_input_path, subst_keys)
    run_doxygen(options.doxygen_path, 'doc/doxyfile', 'doc', is_silent=options.silent)
    if not options.silent:
        print(open(warning_log_path, 'r').read())
    index_path = os.path.abspath(os.path.join('doc', subst_keys['%HTML_OUTPUT%'], 'index.html'))
    print('Generated documentation can be found in:')
    print(index_path)
    if options.open:
        import webbrowser
        webbrowser.open('file://' + index_path)
    if options.make_tarball:
        print('Generating doc tarball to', tarball_path)
        tarball_sources = [
            output_dir,
            'README.md',
            'LICENSE',
            'NEWS.txt',
            'version'
            ]
        tarball_basedir = os.path.join(output_dir, html_output_dirname)
        tarball.make_tarball(tarball_path, tarball_sources, tarball_basedir, html_output_dirname)
    return tarball_path, html_output_dirname

def main():
    usage = """%prog
    Generates doxygen documentation in build/doxygen.
    Optionally makes a tarball of the documentation to dist/.

    Must be started in the project top directory.    
    """
    from optparse import OptionParser
    parser = OptionParser(usage=usage)
    parser.allow_interspersed_args = False
    parser.add_option('--with-dot', dest="with_dot", action='store_true', default=False,
        help="""Enable usage of DOT to generate collaboration diagram""")
    parser.add_option('--dot', dest="dot_path", action='store', default=find_program('dot'),
        help="""Path to GraphViz dot tool. Must be full qualified path. [Default: %default]""")
    parser.add_option('--doxygen', dest="doxygen_path", action='store', default=find_program('doxygen'),
        help="""Path to Doxygen tool. [Default: %default]""")
    parser.add_option('--in', dest="doxyfile_input_path", action='store', default='doc/doxyfile.in',
        help="""Path to doxygen inputs. [Default: %default]""")
    parser.add_option('--with-html-help', dest="with_html_help", action='store_true', default=False,
        help="""Enable generation of Microsoft HTML HELP""")
    parser.add_option('--no-uml-look', dest="with_uml_look", action='store_false', default=True,
        help="""Generates DOT graph without UML look [Default: False]""")
    parser.add_option('--open', dest="open", action='store_true', default=False,
        help="""Open the HTML index in the web browser after generation""")
    parser.add_option('--tarball', dest="make_tarball", action='store_true', default=False,
        help="""Generates a tarball of the documentation in dist/ directory""")
    parser.add_option('-s', '--silent', dest="silent", action='store_true', default=False,
        help="""Hides doxygen output""")
    parser.enable_interspersed_args()
    options, args = parser.parse_args()
    build_doc(options)

if __name__ == '__main__':
    main()
