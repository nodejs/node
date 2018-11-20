# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json
import os.path
import re
import sys

description = ''

primitiveTypes = ['integer', 'number', 'boolean', 'string', 'object', 'any', 'array']


def assignType(item, type, isArray=False):
    if isArray:
        item['type'] = 'array'
        item['items'] = collections.OrderedDict()
        assignType(item['items'], type)
        return

    if type == 'enum':
        type = 'string'
    if type in primitiveTypes:
        item['type'] = type
    else:
        item['$ref'] = type


def createItem(d, experimental, deprecated, name=None):
    result = collections.OrderedDict(d)
    if name:
        result['name'] = name
    global description
    if description:
        result['description'] = description.strip()
    if experimental:
        result['experimental'] = True
    if deprecated:
        result['deprecated'] = True
    return result


def parse(data, file_name):
    protocol = collections.OrderedDict()
    protocol['version'] = collections.OrderedDict()
    protocol['domains'] = []
    domain = None
    item = None
    subitems = None
    nukeDescription = False
    global description
    lines = data.split('\n')
    for i in range(0, len(lines)):
        if nukeDescription:
            description = ''
            nukeDescription = False
        line = lines[i]
        trimLine = line.strip()

        if trimLine.startswith('#'):
            if len(description):
              description += '\n'
            description += trimLine[2:]
            continue
        else:
            nukeDescription = True

        if len(trimLine) == 0:
            continue

        match = re.compile('^(experimental )?(deprecated )?domain (.*)').match(line)
        if match:
            domain = createItem({'domain' : match.group(3)}, match.group(1), match.group(2))
            protocol['domains'].append(domain)
            continue

        match = re.compile('^  depends on ([^\s]+)').match(line)
        if match:
            if 'dependencies' not in domain:
                domain['dependencies'] = []
            domain['dependencies'].append(match.group(1))
            continue

        match = re.compile('^  (experimental )?(deprecated )?type (.*) extends (array of )?([^\s]+)').match(line)
        if match:
            if 'types' not in domain:
                domain['types'] = []
            item = createItem({'id': match.group(3)}, match.group(1), match.group(2))
            assignType(item, match.group(5), match.group(4))
            domain['types'].append(item)
            continue

        match = re.compile('^  (experimental )?(deprecated )?(command|event) (.*)').match(line)
        if match:
            list = []
            if match.group(3) == 'command':
                if 'commands' in domain:
                    list = domain['commands']
                else:
                    list = domain['commands'] = []
            else:
                if 'events' in domain:
                    list = domain['events']
                else:
                    list = domain['events'] = []

            item = createItem({}, match.group(1), match.group(2), match.group(4))
            list.append(item)
            continue

        match = re.compile('^      (experimental )?(deprecated )?(optional )?(array of )?([^\s]+) ([^\s]+)').match(line)
        if match:
            param = createItem({}, match.group(1), match.group(2), match.group(6))
            if match.group(3):
                param['optional'] = True
            assignType(param, match.group(5), match.group(4))
            if match.group(5) == 'enum':
                enumliterals = param['enum'] = []
            subitems.append(param)
            continue

        match = re.compile('^    (parameters|returns|properties)').match(line)
        if match:
            subitems = item[match.group(1)] = []
            continue

        match = re.compile('^    enum').match(line)
        if match:
            enumliterals = item['enum'] = []
            continue

        match = re.compile('^version').match(line)
        if match:
            continue

        match = re.compile('^  major (\d+)').match(line)
        if match:
            protocol['version']['major'] = match.group(1)
            continue

        match = re.compile('^  minor (\d+)').match(line)
        if match:
            protocol['version']['minor'] = match.group(1)
            continue

        match = re.compile('^    redirect ([^\s]+)').match(line)
        if match:
            item['redirect'] = match.group(1)
            continue

        match = re.compile('^      (  )?[^\s]+$').match(line)
        if match:
            # enum literal
            enumliterals.append(trimLine)
            continue

        print 'Error in %s:%s, illegal token: \t%s' % (file_name, i, line)
        sys.exit(1)
    return protocol


def loads(data, file_name):
    if file_name.endswith(".pdl"):
        return parse(data, file_name)
    return json.loads(data)
