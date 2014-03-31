#!/usr/bin/env python

#
# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# Emits a C++ file to be compiled and linked into libv8 to support postmortem
# debugging tools.  Most importantly, this tool emits constants describing V8
# internals:
#
#    v8dbg_type_CLASS__TYPE = VALUE             Describes class type values
#    v8dbg_class_CLASS__FIELD__TYPE = OFFSET    Describes class fields
#    v8dbg_parent_CLASS__PARENT                 Describes class hierarchy
#    v8dbg_frametype_NAME = VALUE               Describes stack frame values
#    v8dbg_off_fp_NAME = OFFSET                 Frame pointer offsets
#    v8dbg_prop_NAME = OFFSET                   Object property offsets
#    v8dbg_NAME = VALUE                         Miscellaneous values
#
# These constants are declared as global integers so that they'll be present in
# the generated libv8 binary.
#

import re
import sys

#
# Miscellaneous constants, tags, and masks used for object identification.
#
consts_misc = [
    { 'name': 'FirstNonstringType',     'value': 'FIRST_NONSTRING_TYPE' },

    { 'name': 'IsNotStringMask',        'value': 'kIsNotStringMask' },
    { 'name': 'StringTag',              'value': 'kStringTag' },
    { 'name': 'NotStringTag',           'value': 'kNotStringTag' },

    { 'name': 'StringEncodingMask',     'value': 'kStringEncodingMask' },
    { 'name': 'TwoByteStringTag',       'value': 'kTwoByteStringTag' },
    { 'name': 'AsciiStringTag',         'value': 'kOneByteStringTag' },

    { 'name': 'StringRepresentationMask',
        'value': 'kStringRepresentationMask' },
    { 'name': 'SeqStringTag',           'value': 'kSeqStringTag' },
    { 'name': 'ConsStringTag',          'value': 'kConsStringTag' },
    { 'name': 'ExternalStringTag',      'value': 'kExternalStringTag' },
    { 'name': 'SlicedStringTag',        'value': 'kSlicedStringTag' },

    { 'name': 'FailureTag',             'value': 'kFailureTag' },
    { 'name': 'FailureTagMask',         'value': 'kFailureTagMask' },
    { 'name': 'HeapObjectTag',          'value': 'kHeapObjectTag' },
    { 'name': 'HeapObjectTagMask',      'value': 'kHeapObjectTagMask' },
    { 'name': 'SmiTag',                 'value': 'kSmiTag' },
    { 'name': 'SmiTagMask',             'value': 'kSmiTagMask' },
    { 'name': 'SmiValueShift',          'value': 'kSmiTagSize' },
    { 'name': 'SmiShiftSize',           'value': 'kSmiShiftSize' },
    { 'name': 'PointerSizeLog2',        'value': 'kPointerSizeLog2' },

    { 'name': 'prop_idx_first',
        'value': 'DescriptorArray::kFirstIndex' },
    { 'name': 'prop_type_field',
        'value': 'FIELD' },
    { 'name': 'prop_type_first_phantom',
        'value': 'TRANSITION' },
    { 'name': 'prop_type_mask',
        'value': 'PropertyDetails::TypeField::kMask' },

    { 'name': 'prop_desc_key',
        'value': 'DescriptorArray::kDescriptorKey' },
    { 'name': 'prop_desc_details',
        'value': 'DescriptorArray::kDescriptorDetails' },
    { 'name': 'prop_desc_value',
        'value': 'DescriptorArray::kDescriptorValue' },
    { 'name': 'prop_desc_size',
        'value': 'DescriptorArray::kDescriptorSize' },

    { 'name': 'off_fp_context',
        'value': 'StandardFrameConstants::kContextOffset' },
    { 'name': 'off_fp_constant_pool',
        'value': 'StandardFrameConstants::kConstantPoolOffset' },
    { 'name': 'off_fp_marker',
        'value': 'StandardFrameConstants::kMarkerOffset' },
    { 'name': 'off_fp_function',
        'value': 'JavaScriptFrameConstants::kFunctionOffset' },
    { 'name': 'off_fp_args',
        'value': 'JavaScriptFrameConstants::kLastParameterOffset' },
];

#
# The following useful fields are missing accessors, so we define fake ones.
#
extras_accessors = [
    'HeapObject, map, Map, kMapOffset',
    'JSObject, elements, Object, kElementsOffset',
    'FixedArray, data, uintptr_t, kHeaderSize',
    'Map, instance_attributes, int, kInstanceAttributesOffset',
    'Map, inobject_properties, int, kInObjectPropertiesOffset',
    'Map, instance_size, int, kInstanceSizeOffset',
    'HeapNumber, value, double, kValueOffset',
    'ConsString, first, String, kFirstOffset',
    'ConsString, second, String, kSecondOffset',
    'ExternalString, resource, Object, kResourceOffset',
    'SeqOneByteString, chars, char, kHeaderSize',
    'SeqTwoByteString, chars, char, kHeaderSize',
    'SharedFunctionInfo, code, Code, kCodeOffset',
    'SlicedString, parent, String, kParentOffset',
    'Code, instruction_start, uintptr_t, kHeaderSize',
    'Code, instruction_size, int, kInstructionSizeOffset',
];

#
# The following is a whitelist of classes we expect to find when scanning the
# source code. This list is not exhaustive, but it's still useful to identify
# when this script gets out of sync with the source. See load_objects().
#
expected_classes = [
    'ConsString', 'FixedArray', 'HeapNumber', 'JSArray', 'JSFunction',
    'JSObject', 'JSRegExp', 'JSValue', 'Map', 'Oddball', 'Script',
    'SeqOneByteString', 'SharedFunctionInfo'
];


#
# The following structures store high-level representations of the structures
# for which we're going to emit descriptive constants.
#
types = {};             # set of all type names
typeclasses = {};       # maps type names to corresponding class names
klasses = {};           # known classes, including parents
fields = [];            # field declarations

header = '''
/*
 * This file is generated by %s.  Do not edit directly.
 */

#include "v8.h"
#include "frames.h"
#include "frames-inl.h" /* for architecture-specific frame constants */

using namespace v8::internal;

extern "C" {

/* stack frame constants */
#define FRAME_CONST(value, klass)       \
    int v8dbg_frametype_##klass = StackFrame::value;

STACK_FRAME_TYPE_LIST(FRAME_CONST)

#undef FRAME_CONST

''' % sys.argv[0];

footer = '''
}
'''

#
# Loads class hierarchy and type information from "objects.h".
#
def load_objects():
        objfilename = sys.argv[2];
        objfile = open(objfilename, 'r');
        in_insttype = False;

        typestr = '';

        #
        # Construct a dictionary for the classes we're sure should be present.
        #
        checktypes = {};
        for klass in expected_classes:
                checktypes[klass] = True;

        #
        # Iterate objects.h line-by-line to collect type and class information.
        # For types, we accumulate a string representing the entire InstanceType
        # enum definition and parse it later because it's easier to do so
        # without the embedded newlines.
        #
        for line in objfile:
                if (line.startswith('enum InstanceType {')):
                        in_insttype = True;
                        continue;

                if (in_insttype and line.startswith('};')):
                        in_insttype = False;
                        continue;

                line = re.sub('//.*', '', line.rstrip().lstrip());

                if (in_insttype):
                        typestr += line;
                        continue;

                match = re.match('class (\w[^\s:]*)(: public (\w[^\s{]*))?\s*{',
                    line);

                if (match):
                        klass = match.group(1);
                        pklass = match.group(3);
                        klasses[klass] = { 'parent': pklass };

        #
        # Process the instance type declaration.
        #
        entries = typestr.split(',');
        for entry in entries:
                types[re.sub('\s*=.*', '', entry).lstrip()] = True;

        #
        # Infer class names for each type based on a systematic transformation.
        # For example, "JS_FUNCTION_TYPE" becomes "JSFunction".  We find the
        # class for each type rather than the other way around because there are
        # fewer cases where one type maps to more than one class than the other
        # way around.
        #
        for type in types:
                #
                # Symbols and Strings are implemented using the same classes.
                #
                usetype = re.sub('SYMBOL_', 'STRING_', type);

                #
                # REGEXP behaves like REG_EXP, as in JS_REGEXP_TYPE => JSRegExp.
                #
                usetype = re.sub('_REGEXP_', '_REG_EXP_', usetype);

                #
                # Remove the "_TYPE" suffix and then convert to camel case,
                # except that a "JS" prefix remains uppercase (as in
                # "JS_FUNCTION_TYPE" => "JSFunction").
                #
                if (not usetype.endswith('_TYPE')):
                        continue;

                usetype = usetype[0:len(usetype) - len('_TYPE')];
                parts = usetype.split('_');
                cctype = '';

                if (parts[0] == 'JS'):
                        cctype = 'JS';
                        start = 1;
                else:
                        cctype = '';
                        start = 0;

                for ii in range(start, len(parts)):
                        part = parts[ii];
                        cctype += part[0].upper() + part[1:].lower();

                #
                # Mapping string types is more complicated.  Both types and
                # class names for Strings specify a representation (e.g., Seq,
                # Cons, External, or Sliced) and an encoding (TwoByte or Ascii),
                # In the simplest case, both of these are explicit in both
                # names, as in:
                #
                #       EXTERNAL_ASCII_STRING_TYPE => ExternalAsciiString
                #
                # However, either the representation or encoding can be omitted
                # from the type name, in which case "Seq" and "TwoByte" are
                # assumed, as in:
                #
                #       STRING_TYPE => SeqTwoByteString
                #
                # Additionally, sometimes the type name has more information
                # than the class, as in:
                #
                #       CONS_ASCII_STRING_TYPE => ConsString
                #
                # To figure this out dynamically, we first check for a
                # representation and encoding and add them if they're not
                # present.  If that doesn't yield a valid class name, then we
                # strip out the representation.
                #
                if (cctype.endswith('String')):
                        if (cctype.find('Cons') == -1 and
                            cctype.find('External') == -1 and
                            cctype.find('Sliced') == -1):
                                if (cctype.find('Ascii') != -1):
                                        cctype = re.sub('AsciiString$',
                                            'SeqOneByteString', cctype);
                                else:
                                        cctype = re.sub('String$',
                                            'SeqString', cctype);

                        if (cctype.find('Ascii') == -1):
                                cctype = re.sub('String$', 'TwoByteString',
                                    cctype);

                        if (not (cctype in klasses)):
                                cctype = re.sub('Ascii', '', cctype);
                                cctype = re.sub('TwoByte', '', cctype);

                #
                # Despite all that, some types have no corresponding class.
                #
                if (cctype in klasses):
                        typeclasses[type] = cctype;
                        if (cctype in checktypes):
                                del checktypes[cctype];

        if (len(checktypes) > 0):
                for klass in checktypes:
                        print('error: expected class \"%s\" not found' % klass);

                sys.exit(1);


#
# For a given macro call, pick apart the arguments and return an object
# describing the corresponding output constant.  See load_fields().
#
def parse_field(call):
        # Replace newlines with spaces.
        for ii in range(0, len(call)):
                if (call[ii] == '\n'):
                        call[ii] == ' ';

        idx = call.find('(');
        kind = call[0:idx];
        rest = call[idx + 1: len(call) - 1];
        args = re.split('\s*,\s*', rest);

        consts = [];

        if (kind == 'ACCESSORS' or kind == 'ACCESSORS_GCSAFE'):
                klass = args[0];
                field = args[1];
                dtype = args[2];
                offset = args[3];

                return ({
                    'name': 'class_%s__%s__%s' % (klass, field, dtype),
                    'value': '%s::%s' % (klass, offset)
                });

        assert(kind == 'SMI_ACCESSORS');
        klass = args[0];
        field = args[1];
        offset = args[2];

        return ({
            'name': 'class_%s__%s__%s' % (klass, field, 'SMI'),
            'value': '%s::%s' % (klass, offset)
        });

#
# Load field offset information from objects-inl.h.
#
def load_fields():
        inlfilename = sys.argv[3];
        inlfile = open(inlfilename, 'r');

        #
        # Each class's fields and the corresponding offsets are described in the
        # source by calls to macros like "ACCESSORS" (and friends).  All we do
        # here is extract these macro invocations, taking into account that they
        # may span multiple lines and may contain nested parentheses.  We also
        # call parse_field() to pick apart the invocation.
        #
        prefixes = [ 'ACCESSORS', 'ACCESSORS_GCSAFE', 'SMI_ACCESSORS' ];
        current = '';
        opens = 0;

        for line in inlfile:
                if (opens > 0):
                        # Continuation line
                        for ii in range(0, len(line)):
                                if (line[ii] == '('):
                                        opens += 1;
                                elif (line[ii] == ')'):
                                        opens -= 1;

                                if (opens == 0):
                                        break;

                        current += line[0:ii + 1];
                        continue;

                for prefix in prefixes:
                        if (not line.startswith(prefix + '(')):
                                continue;

                        if (len(current) > 0):
                                fields.append(parse_field(current));
                                current = '';

                        for ii in range(len(prefix), len(line)):
                                if (line[ii] == '('):
                                        opens += 1;
                                elif (line[ii] == ')'):
                                        opens -= 1;

                                if (opens == 0):
                                        break;

                        current += line[0:ii + 1];

        if (len(current) > 0):
                fields.append(parse_field(current));
                current = '';

        for body in extras_accessors:
                fields.append(parse_field('ACCESSORS(%s)' % body));

#
# Emit a block of constants.
#
def emit_set(out, consts):
        # Fix up overzealous parses.  This could be done inside the
        # parsers but as there are several, it's easiest to do it here.
        ws = re.compile('\s+')
        for const in consts:
                name = ws.sub('', const['name'])
                value = ws.sub('', str(const['value']))  # Can be a number.
                out.write('int v8dbg_%s = %s;\n' % (name, value))
        out.write('\n');

#
# Emit the whole output file.
#
def emit_config():
        out = file(sys.argv[1], 'w');

        out.write(header);

        out.write('/* miscellaneous constants */\n');
        emit_set(out, consts_misc);

        out.write('/* class type information */\n');
        consts = [];
        keys = typeclasses.keys();
        keys.sort();
        for typename in keys:
                klass = typeclasses[typename];
                consts.append({
                    'name': 'type_%s__%s' % (klass, typename),
                    'value': typename
                });

        emit_set(out, consts);

        out.write('/* class hierarchy information */\n');
        consts = [];
        keys = klasses.keys();
        keys.sort();
        for klassname in keys:
                pklass = klasses[klassname]['parent'];
                if (pklass == None):
                        continue;

                consts.append({
                    'name': 'parent_%s__%s' % (klassname, pklass),
                    'value': 0
                });

        emit_set(out, consts);

        out.write('/* field information */\n');
        emit_set(out, fields);

        out.write(footer);

if (len(sys.argv) < 4):
        print('usage: %s output.cc objects.h objects-inl.h' % sys.argv[0]);
        sys.exit(2);

load_objects();
load_fields();
emit_config();
