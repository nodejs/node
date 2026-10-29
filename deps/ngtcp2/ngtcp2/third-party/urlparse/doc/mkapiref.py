#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# urlparse
#
# Copyright (c) 2024 urlparse contributors
# Copyright (c) 2023 sfparse contributors
# Copyright (c) 2020 nghttp3 contributors
# Copyright (c) 2020 ngtcp2 contributors
# Copyright (c) 2012 Tatsuhiro Tsujikawa
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:

# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Generates API reference from C source code.

import re, sys, argparse, os.path

class FunctionDoc:
    def __init__(self, name, content, domain, filename):
        self.name = name
        self.content = content
        self.domain = domain
        if self.domain == 'function':
            self.funcname = re.search(r'(urlparse_[^ )]+)\(', self.name).group(1)
        self.filename = filename

    def write(self, out):
        out.write('.. {}:: {}\n'.format(self.domain, self.name))
        out.write('\n')
        for line in self.content:
            out.write('    {}\n'.format(line))

class StructDoc:
    def __init__(self, name, content, members, member_domain):
        self.name = name
        self.content = content
        self.members = members
        self.member_domain = member_domain

    def write(self, out):
        indents = '    '
        if self.name:
            out.write('.. type:: {}\n'.format(self.name))
            out.write('\n')
            for line in self.content:
                out.write('    {}\n'.format(line))
            out.write('\n')
            for name, content, indent in self.members:
                if name.startswith('@union_'):
                    out.write('{}    .. union:: {}\n'.format(indents * indent, name))
                if name.startswith('@struct_'):
                    out.write('{}    .. {}:: struct {}\n'.format(indents * indent, self.member_domain, name))
                else:
                    out.write('{}    .. {}:: {}\n'.format(indents * indent, self.member_domain, name))
                out.write('\n')
                for line in content:
                    out.write('{}        {}\n'.format(indents * indent, line))
                if name.startswith('@struct_'):
                    out.write('\n')
            out.write('\n')

class EnumDoc:
    def __init__(self, name, content, members):
        self.name = name
        self.content = content
        self.members = members

    def write(self, out):
        if self.name:
            out.write('.. type:: {}\n'.format(self.name))
            out.write('\n')
            for line in self.content:
                out.write('    {}\n'.format(line))
            out.write('\n')
            for name, content in self.members:
                out.write('    .. enum:: {}\n'.format(name))
                out.write('\n')
                for line in content:
                    out.write('        {}\n'.format(line))
            out.write('\n')

class MacroDoc:
    def __init__(self, name, content):
        self.name = name
        self.content = content

    def write(self, out):
        out.write('''.. macro:: {}\n'''.format(self.name))
        out.write('\n')
        for line in self.content:
            out.write('    {}\n'.format(line))

class MacroSectionDoc:
    def __init__(self, content):
        self.content = content

    def write(self, out):
        out.write('\n')
        c = ' '.join(self.content).strip()
        out.write(c)
        out.write('\n')
        out.write('-' * len(c))
        out.write('\n\n')

class TypedefDoc:
    def __init__(self, name, content):
        self.name = name
        self.content = content

    def write(self, out):
        out.write('''.. type:: {}\n'''.format(self.name))
        out.write('\n')
        for line in self.content:
            out.write('    {}\n'.format(line))

def make_api_ref(infile):
    macros = []
    enums = []
    types = []
    functions = []
    while True:
        line = infile.readline()
        if not line:
            break
        elif line == '/**\n':
            line = infile.readline()
            doctype = line.split()[1]
            if doctype == '@function':
                functions.append(process_function('function', infile))
            elif doctype == '@functypedef':
                types.append(process_function('type', infile))
            elif doctype == '@struct' or doctype == '@union':
                types.append(process_struct(infile))
            elif doctype == '@enum':
                enums.append(process_enum(infile))
            elif doctype == '@macro':
                macros.append(process_macro(infile))
            elif doctype == '@macrosection':
                macros.append(process_macrosection(infile))
            elif doctype == '@typedef':
                types.append(process_typedef(infile))
    return macros, enums, types, functions

def output(
        title, indexfile, macrosfile, enumsfile, typesfile, funcsdir,
        macros, enums, types, functions):
    indexfile.write('''
{title}
{titledecoration}

.. toctree::
   :maxdepth: 1

   {macros}
   {enums}
   {types}
'''.format(
    title=title, titledecoration='='*len(title),
    macros=os.path.splitext(os.path.basename(macrosfile.name))[0],
    enums=os.path.splitext(os.path.basename(enumsfile.name))[0],
    types=os.path.splitext(os.path.basename(typesfile.name))[0],
))

    for doc in functions:
        indexfile.write('   {}\n'.format(doc.funcname))

    macrosfile.write('''
Macros
======
''')
    for doc in macros:
        doc.write(macrosfile)

    enumsfile.write('''
Enums
=====
''')
    for doc in enums:
        doc.write(enumsfile)

    typesfile.write('''
Types (structs, unions and typedefs)
====================================
''')
    for doc in types:
        doc.write(typesfile)

    for doc in functions:
        with open(os.path.join(funcsdir, doc.funcname + '.rst'), 'w') as f:
            f.write('''
{funcname}
{secul}

Synopsis
--------

*#include <{filename}>*

'''.format(funcname=doc.funcname, secul='='*len(doc.funcname),
           filename=doc.filename))
            doc.write(f)

def process_macro(infile):
    content = read_content(infile)
    lines = []
    while True:
        line = infile.readline()
        if not line:
            break
        line = line.rstrip()
        lines.append(line.rstrip('\\'))
        if not line.endswith('\\'):
            break

    macro_name = re.sub(r'#define ', '', ''.join(lines))
    m = re.match(r'^[^( ]+(:?\(.*?\))?', macro_name)
    macro_name =  m.group(0)
    return MacroDoc(macro_name, content)

def process_macrosection(infile):
    content = read_content(infile)
    return MacroSectionDoc(content)

def process_typedef(infile):
    content = read_content(infile)
    typedef = infile.readline()
    typedef = re.sub(r';\n$', '', typedef)
    typedef = re.sub(r'typedef ', '', typedef)
    return TypedefDoc(typedef, content)

def process_enum(infile):
    members = []
    enum_name = None
    content = read_content(infile)
    while True:
        line = infile.readline()
        if not line:
            break
        elif re.match(r'\s*/\*\*\n', line):
            member_content = read_content(infile)
            line = infile.readline()
            items = line.split()
            member_name = items[0].rstrip(',')
            if len(items) >= 3:
                member_content.insert(0, '(``{}``) '\
                                      .format(' '.join(items[2:]).rstrip(',')))
            members.append((member_name, member_content))
        elif line.startswith('}'):
            enum_name = line.rstrip().split()[1]
            enum_name = re.sub(r';$', '', enum_name)
            break
    return EnumDoc(enum_name, content, members)

def process_struct(infile):
    members = []
    struct_name = None
    content = read_content(infile)
    indent = 0
    anonstruct_name = None
    anonstruct_member_name = None
    anonstruct_member_content = None
    anonstruct_members = []
    while True:
        line = infile.readline()
        if not line:
            break
        elif re.match(r'\s*/\*\*\n', line):
            member_content = read_content(infile)
            if '@anonunion_start' in member_content:
                member_name = member_content[2].strip()
                members.append((member_name, '', indent))
                indent += 1
            elif '@anonunion_end' in member_content:
                indent -= 1
            elif '@anonstruct_start' in member_content:
                anonstruct_name = member_content[2].strip()
                anonstruct_member_content = member_content[4:]
                indent += 1
            elif '@anonstruct_end' in member_content:
                line = infile.readline()
                anonstruct_member_name = re.sub(r'\s*}\s*', '', line).rstrip().rstrip(';')
                indent -= 1
                members.append((anonstruct_name + ' ' + anonstruct_member_name,
                                anonstruct_member_content, indent))
                members.extend(anonstruct_members)
                anonstruct_name = None
                anonstruct_member_name = None
                anonstruct_member_content = None
                anonstruct_members = []
            else:
                line = infile.readline()
                member_name = line.rstrip().rstrip(';')
                if anonstruct_name is None:
                    members.append((member_name, member_content, indent))
                else:
                    anonstruct_members.append((member_name, member_content, indent))
        elif line.startswith('}') or\
                (line.startswith('typedef ') and line.endswith(';\n')):
            if line.startswith('}'):
                index = 1
            else:
                index = 3
            struct_name = line.rstrip().split()[index]
            struct_name = re.sub(r';$', '', struct_name)
            break
    return StructDoc(struct_name, content, members, 'member')

def process_function(domain, infile):
    content = read_content(infile)
    func_proto = []
    while True:
        line = infile.readline()
        if not line:
            break
        elif line == '\n':
            break
        else:
            func_proto.append(line)
    func_proto = ''.join(func_proto)
    func_proto = re.sub(r'int (settings|callbacks)_version,',
                        '', func_proto)
    func_proto = re.sub(r'_versioned\(', '(', func_proto)
    func_proto = re.sub(r';\n$', '', func_proto)
    func_proto = re.sub(r'\s+', ' ', func_proto)
    func_proto = re.sub(r'typedef ', '', func_proto)
    filename = os.path.basename(infile.name)
    return FunctionDoc(func_proto, content, domain, filename)

def read_content(infile):
    content = []
    while True:
        line = infile.readline()
        if not line:
            break
        if re.match(r'\s*\*/\n', line):
            break
        else:
            content.append(transform_content(line.rstrip()))
    return content

def arg_repl(matchobj):
    return '*{}*'.format(matchobj.group(1).replace('*', '\\*'))

def transform_content(content):
    content = re.sub(r'^\s+\* ?', '', content)
    content = re.sub(r'\|([^\s|]+)\|', arg_repl, content)
    return content

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generate API reference")
    parser.add_argument('--title', default='API Reference',
                        help='title of index page')
    parser.add_argument('index', type=argparse.FileType('w'),
                        help='index output file')
    parser.add_argument('macros', type=argparse.FileType('w'),
                        help='macros section output file.  The filename should be macros.rst')
    parser.add_argument('enums', type=argparse.FileType('w'),
                        help='enums section output file.  The filename should be enums.rst')
    parser.add_argument('types', type=argparse.FileType('w'),
                        help='types section output file.  The filename should be types.rst')
    parser.add_argument('funcsdir',
                        help='functions doc output dir')
    parser.add_argument('files', nargs='+', type=argparse.FileType('r'),
                        help='source file')
    args = parser.parse_args()
    macros = []
    enums = []
    types = []
    funcs = []
    for infile in args.files:
        m, e, t, f = make_api_ref(infile)
        macros.extend(m)
        enums.extend(e)
        types.extend(t)
        funcs.extend(f)
    funcs.sort(key=lambda x: x.funcname)
    output(
        args.title,
        args.index, args.macros, args.enums, args.types, args.funcsdir,
        macros, enums, types, funcs)
