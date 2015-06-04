#!/usr/bin/env python
import subprocess
import re
import os
import errno
import collections
import sys

class Platform(object):
    pass

sdk_re = re.compile(r'.*-sdk ([a-zA-Z0-9.]*)')

def sdkinfo(sdkname):
    ret = {}
    for line in subprocess.Popen(['xcodebuild', '-sdk', sdkname, '-version'], stdout=subprocess.PIPE).stdout:
        kv = line.strip().split(': ', 1)
        if len(kv) == 2:
            k,v = kv
            ret[k] = v
    return ret

desktop_sdk_info = sdkinfo('macosx')

def latest_sdks():
    latest_desktop = None
    for line in subprocess.Popen(['xcodebuild', '-showsdks'], stdout=subprocess.PIPE).stdout:
        match = sdk_re.match(line)
        if match:
            if 'OS X' in line:
                latest_desktop = match.group(1)

    return latest_desktop

desktop_sdk = latest_sdks()

class desktop_platform_32(Platform):
    sdk='macosx'
    arch = 'i386'
    name = 'mac32'
    triple = 'i386-apple-darwin10'
    sdkroot = desktop_sdk_info['Path']

    prefix = "#if defined(__i386__) && !defined(__x86_64__)\n\n"
    suffix = "\n\n#endif"

class desktop_platform_64(Platform):
    sdk='macosx'
    arch = 'x86_64'
    name = 'mac'
    triple = 'x86_64-apple-darwin10'
    sdkroot = desktop_sdk_info['Path']

    prefix = "#if !defined(__i386__) && defined(__x86_64__)\n\n"
    suffix = "\n\n#endif"

def move_file(src_dir, dst_dir, filename, file_suffix=None, prefix='', suffix=''):
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)

    out_filename = filename

    if file_suffix:
        split_name = os.path.splitext(filename)
        out_filename =  "%s_%s%s" % (split_name[0], file_suffix, split_name[1])

    with open(os.path.join(src_dir, filename)) as in_file:
        with open(os.path.join(dst_dir, out_filename), 'w') as out_file:
            if prefix:
                out_file.write(prefix)

            out_file.write(in_file.read())

            if suffix:
                out_file.write(suffix)

headers_seen = collections.defaultdict(set)

def move_source_tree(src_dir, dest_dir, dest_include_dir, arch=None, prefix=None, suffix=None):
    for root, dirs, files in os.walk(src_dir, followlinks=True):
        relroot = os.path.relpath(root,src_dir)

        def move_dir(arch, prefix='', suffix='', files=[]):
            for file in files:
                file_suffix = None
                if file.endswith('.h'):
                    if dest_include_dir:
                        file_suffix = arch
                        if arch:
                            headers_seen[file].add(arch)
                        move_file(root, dest_include_dir, file, arch, prefix=prefix, suffix=suffix)

                elif dest_dir:
                    outroot = os.path.join(dest_dir, relroot)
                    move_file(root, outroot, file, prefix=prefix, suffix=suffix)

        if relroot == '.':
            move_dir(arch=arch,
                     files=files,
                     prefix=prefix,
                     suffix=suffix)
        elif relroot == 'x86':
            move_dir(arch='i386',
                     prefix="#if defined(__i386__) && !defined(__x86_64__)\n\n",
                     suffix="\n\n#endif",
                     files=files)
            move_dir(arch='x86_64',
                     prefix="#if !defined(__i386__) && defined(__x86_64__)\n\n",
                     suffix="\n\n#endif",
                     files=files)

def build_target(platform):
    def xcrun_cmd(cmd):
        return subprocess.check_output(['xcrun', '-sdk', platform.sdkroot, '-find', cmd]).strip()

    build_dir = 'build_' + platform.name
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
        env = dict(CC=xcrun_cmd('clang'),
                   LD=xcrun_cmd('ld'),
                   CFLAGS='-arch %s -isysroot %s -mmacosx-version-min=10.6' % (platform.arch, platform.sdkroot))
        working_dir=os.getcwd()
        try:
            os.chdir(build_dir)
            subprocess.check_call(['../configure', '-host', platform.triple], env=env)
            move_source_tree('.', None, '../osx/include',
                             arch=platform.arch,
                             prefix=platform.prefix,
                             suffix=platform.suffix)
            move_source_tree('./include', None, '../osx/include',
                             arch=platform.arch,
                             prefix=platform.prefix,
                             suffix=platform.suffix)
        finally:
            os.chdir(working_dir)

        for header_name, archs in headers_seen.iteritems():
            basename, suffix = os.path.splitext(header_name)

def main():
    move_source_tree('src', 'osx/src', 'osx/include')
    move_source_tree('include', None, 'osx/include')
    build_target(desktop_platform_32)
    build_target(desktop_platform_64)

    for header_name, archs in headers_seen.iteritems():
        basename, suffix = os.path.splitext(header_name)
        with open(os.path.join('osx/include', header_name), 'w') as header:
            for arch in archs:
                header.write('#include <%s_%s%s>\n' % (basename, arch, suffix))

if __name__ == '__main__':
    main()
