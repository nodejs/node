from __future__ import print_function
import collections
import itertools
import json
import os
import os.path
import re
import shutil
import string
import subprocess
import sys
import cgi

class BuildDesc:
    def __init__(self, prepend_envs=None, variables=None, build_type=None, generator=None):
        self.prepend_envs = prepend_envs or [] # [ { "var": "value" } ]
        self.variables = variables or []
        self.build_type = build_type
        self.generator = generator

    def merged_with(self, build_desc):
        """Returns a new BuildDesc by merging field content.
           Prefer build_desc fields to self fields for single valued field.
        """
        return BuildDesc(self.prepend_envs + build_desc.prepend_envs,
                          self.variables + build_desc.variables,
                          build_desc.build_type or self.build_type,
                          build_desc.generator or self.generator)

    def env(self):
        environ = os.environ.copy()
        for values_by_name in self.prepend_envs:
            for var, value in list(values_by_name.items()):
                var = var.upper()
                if type(value) is unicode:
                    value = value.encode(sys.getdefaultencoding())
                if var in environ:
                    environ[var] = value + os.pathsep + environ[var]
                else:
                    environ[var] = value
        return environ

    def cmake_args(self):
        args = ["-D%s" % var for var in self.variables]
        # skip build type for Visual Studio solution as it cause warning
        if self.build_type and 'Visual' not in self.generator:
            args.append("-DCMAKE_BUILD_TYPE=%s" % self.build_type)
        if self.generator:
            args.extend(['-G', self.generator])
        return args

    def __repr__(self):
        return "BuildDesc(%s, build_type=%s)" %  (" ".join(self.cmake_args()), self.build_type)

class BuildData:
    def __init__(self, desc, work_dir, source_dir):
        self.desc = desc
        self.work_dir = work_dir
        self.source_dir = source_dir
        self.cmake_log_path = os.path.join(work_dir, 'batchbuild_cmake.log')
        self.build_log_path = os.path.join(work_dir, 'batchbuild_build.log')
        self.cmake_succeeded = False
        self.build_succeeded = False

    def execute_build(self):
        print('Build %s' % self.desc)
        self._make_new_work_dir()
        self.cmake_succeeded = self._generate_makefiles()
        if self.cmake_succeeded:
            self.build_succeeded = self._build_using_makefiles()
        return self.build_succeeded

    def _generate_makefiles(self):
        print('  Generating makefiles: ', end=' ')
        cmd = ['cmake'] + self.desc.cmake_args() + [os.path.abspath(self.source_dir)]
        succeeded = self._execute_build_subprocess(cmd, self.desc.env(), self.cmake_log_path)
        print('done' if succeeded else 'FAILED')
        return succeeded

    def _build_using_makefiles(self):
        print('  Building:', end=' ')
        cmd = ['cmake', '--build', self.work_dir]
        if self.desc.build_type:
            cmd += ['--config', self.desc.build_type]
        succeeded = self._execute_build_subprocess(cmd, self.desc.env(), self.build_log_path)
        print('done' if succeeded else 'FAILED')
        return succeeded

    def _execute_build_subprocess(self, cmd, env, log_path):
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=self.work_dir,
                                    env=env)
        stdout, _ = process.communicate()
        succeeded = (process.returncode == 0)
        with open(log_path, 'wb') as flog:
            log = ' '.join(cmd) + '\n' + stdout + '\nExit code: %r\n' % process.returncode
            flog.write(fix_eol(log))
        return succeeded

    def _make_new_work_dir(self):
        if os.path.isdir(self.work_dir):
            print('  Removing work directory', self.work_dir)
            shutil.rmtree(self.work_dir, ignore_errors=True)
        if not os.path.isdir(self.work_dir):
            os.makedirs(self.work_dir)

def fix_eol(stdout):
    """Fixes wrong EOL produced by cmake --build on Windows (\r\r\n instead of \r\n).
    """
    return re.sub('\r*\n', os.linesep, stdout)

def load_build_variants_from_config(config_path):
    with open(config_path, 'rb') as fconfig:
        data = json.load(fconfig)
    variants = data[ 'cmake_variants' ]
    build_descs_by_axis = collections.defaultdict(list)
    for axis in variants:
        axis_name = axis["name"]
        build_descs = []
        if "generators" in axis:
            for generator_data in axis["generators"]:
                for generator in generator_data["generator"]:
                    build_desc = BuildDesc(generator=generator,
                                            prepend_envs=generator_data.get("env_prepend"))
                    build_descs.append(build_desc)
        elif "variables" in axis:
            for variables in axis["variables"]:
                build_desc = BuildDesc(variables=variables)
                build_descs.append(build_desc)
        elif "build_types" in axis:
            for build_type in axis["build_types"]:
                build_desc = BuildDesc(build_type=build_type)
                build_descs.append(build_desc)
        build_descs_by_axis[axis_name].extend(build_descs)
    return build_descs_by_axis

def generate_build_variants(build_descs_by_axis):
    """Returns a list of BuildDesc generated for the partial BuildDesc for each axis."""
    axis_names = list(build_descs_by_axis.keys())
    build_descs = []
    for axis_name, axis_build_descs in list(build_descs_by_axis.items()):
        if len(build_descs):
            # for each existing build_desc and each axis build desc, create a new build_desc
            new_build_descs = []
            for prototype_build_desc, axis_build_desc in itertools.product(build_descs, axis_build_descs):
                new_build_descs.append(prototype_build_desc.merged_with(axis_build_desc))
            build_descs = new_build_descs
        else:
            build_descs = axis_build_descs
    return build_descs

HTML_TEMPLATE = string.Template('''<html>
<head>
    <title>$title</title>
    <style type="text/css">
    td.failed {background-color:#f08080;}
    td.ok {background-color:#c0eec0;}
    </style>
</head>
<body>
<table border="1">
<thead>
    <tr>
        <th>Variables</th>
        $th_vars
    </tr>
    <tr>
        <th>Build type</th>
        $th_build_types
    </tr>
</thead>
<tbody>
$tr_builds
</tbody>
</table>
</body></html>''')

def generate_html_report(html_report_path, builds):
    report_dir = os.path.dirname(html_report_path)
    # Vertical axis: generator
    # Horizontal: variables, then build_type
    builds_by_generator = collections.defaultdict(list)
    variables = set()
    build_types_by_variable = collections.defaultdict(set)
    build_by_pos_key = {} # { (generator, var_key, build_type): build }
    for build in builds:
        builds_by_generator[build.desc.generator].append(build)
        var_key = tuple(sorted(build.desc.variables))
        variables.add(var_key)
        build_types_by_variable[var_key].add(build.desc.build_type)
        pos_key = (build.desc.generator, var_key, build.desc.build_type)
        build_by_pos_key[pos_key] = build
    variables = sorted(variables)
    th_vars = []
    th_build_types = []
    for variable in variables:
        build_types = sorted(build_types_by_variable[variable])
        nb_build_type = len(build_types_by_variable[variable])
        th_vars.append('<th colspan="%d">%s</th>' % (nb_build_type, cgi.escape(' '.join(variable))))
        for build_type in build_types:
            th_build_types.append('<th>%s</th>' % cgi.escape(build_type))
    tr_builds = []
    for generator in sorted(builds_by_generator):
        tds = [ '<td>%s</td>\n' % cgi.escape(generator) ]
        for variable in variables:
            build_types = sorted(build_types_by_variable[variable])
            for build_type in build_types:
                pos_key = (generator, variable, build_type)
                build = build_by_pos_key.get(pos_key)
                if build:
                    cmake_status = 'ok' if build.cmake_succeeded else 'FAILED'
                    build_status = 'ok' if build.build_succeeded else 'FAILED'
                    cmake_log_url = os.path.relpath(build.cmake_log_path, report_dir)
                    build_log_url = os.path.relpath(build.build_log_path, report_dir)
                    td = '<td class="%s"><a href="%s" class="%s">CMake: %s</a>' % (                        build_status.lower(), cmake_log_url, cmake_status.lower(), cmake_status)
                    if build.cmake_succeeded:
                        td += '<br><a href="%s" class="%s">Build: %s</a>' % (                            build_log_url, build_status.lower(), build_status)
                    td += '</td>'
                else:
                    td = '<td></td>'
                tds.append(td)
        tr_builds.append('<tr>%s</tr>' % '\n'.join(tds))
    html = HTML_TEMPLATE.substitute(        title='Batch build report',
        th_vars=' '.join(th_vars),
        th_build_types=' '.join(th_build_types),
        tr_builds='\n'.join(tr_builds))
    with open(html_report_path, 'wt') as fhtml:
        fhtml.write(html)
    print('HTML report generated in:', html_report_path)

def main():
    usage = r"""%prog WORK_DIR SOURCE_DIR CONFIG_JSON_PATH [CONFIG2_JSON_PATH...]
Build a given CMake based project located in SOURCE_DIR with multiple generators/options.dry_run
as described in CONFIG_JSON_PATH building in WORK_DIR.

Example of call:
python devtools\batchbuild.py e:\buildbots\jsoncpp\build . devtools\agent_vmw7.json
"""
    from optparse import OptionParser
    parser = OptionParser(usage=usage)
    parser.allow_interspersed_args = True
#    parser.add_option('-v', '--verbose', dest="verbose", action='store_true',
#        help="""Be verbose.""")
    parser.enable_interspersed_args()
    options, args = parser.parse_args()
    if len(args) < 3:
        parser.error("Missing one of WORK_DIR SOURCE_DIR CONFIG_JSON_PATH.")
    work_dir = args[0]
    source_dir = args[1].rstrip('/\\')
    config_paths = args[2:]
    for config_path in config_paths:
        if not os.path.isfile(config_path):
            parser.error("Can not read: %r" % config_path)

    # generate build variants
    build_descs = []
    for config_path in config_paths:
        build_descs_by_axis = load_build_variants_from_config(config_path)
        build_descs.extend(generate_build_variants(build_descs_by_axis))
    print('Build variants (%d):' % len(build_descs))
    # assign build directory for each variant
    if not os.path.isdir(work_dir):
        os.makedirs(work_dir)
    builds = []
    with open(os.path.join(work_dir, 'matrix-dir-map.txt'), 'wt') as fmatrixmap:
        for index, build_desc in enumerate(build_descs):
            build_desc_work_dir = os.path.join(work_dir, '%03d' % (index+1))
            builds.append(BuildData(build_desc, build_desc_work_dir, source_dir))
            fmatrixmap.write('%s: %s\n' % (build_desc_work_dir, build_desc))
    for build in builds:
        build.execute_build()
    html_report_path = os.path.join(work_dir, 'batchbuild-report.html')
    generate_html_report(html_report_path, builds)
    print('Done')


if __name__ == '__main__':
    main()

