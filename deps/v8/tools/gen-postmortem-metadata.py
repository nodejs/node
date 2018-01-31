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
# Miscellaneous constants such as tags and masks used for object identification,
# enumeration values used as indexes in internal tables, etc..
#
consts_misc = [
    { 'name': 'FirstNonstringType',     'value': 'FIRST_NONSTRING_TYPE' },
    { 'name': 'APIObjectType',          'value': 'JS_API_OBJECT_TYPE' },
    { 'name': 'SpecialAPIObjectType',   'value': 'JS_SPECIAL_API_OBJECT_TYPE' },

    { 'name': 'IsNotStringMask',        'value': 'kIsNotStringMask' },
    { 'name': 'StringTag',              'value': 'kStringTag' },

    { 'name': 'StringEncodingMask',     'value': 'kStringEncodingMask' },
    { 'name': 'TwoByteStringTag',       'value': 'kTwoByteStringTag' },
    { 'name': 'OneByteStringTag',       'value': 'kOneByteStringTag' },

    { 'name': 'StringRepresentationMask',
        'value': 'kStringRepresentationMask' },
    { 'name': 'SeqStringTag',           'value': 'kSeqStringTag' },
    { 'name': 'ConsStringTag',          'value': 'kConsStringTag' },
    { 'name': 'ExternalStringTag',      'value': 'kExternalStringTag' },
    { 'name': 'SlicedStringTag',        'value': 'kSlicedStringTag' },
    { 'name': 'ThinStringTag',          'value': 'kThinStringTag' },

    { 'name': 'HeapObjectTag',          'value': 'kHeapObjectTag' },
    { 'name': 'HeapObjectTagMask',      'value': 'kHeapObjectTagMask' },
    { 'name': 'SmiTag',                 'value': 'kSmiTag' },
    { 'name': 'SmiTagMask',             'value': 'kSmiTagMask' },
    { 'name': 'SmiValueShift',          'value': 'kSmiTagSize' },
    { 'name': 'SmiShiftSize',           'value': 'kSmiShiftSize' },
    { 'name': 'PointerSizeLog2',        'value': 'kPointerSizeLog2' },

    { 'name': 'OddballFalse',           'value': 'Oddball::kFalse' },
    { 'name': 'OddballTrue',            'value': 'Oddball::kTrue' },
    { 'name': 'OddballTheHole',         'value': 'Oddball::kTheHole' },
    { 'name': 'OddballNull',            'value': 'Oddball::kNull' },
    { 'name': 'OddballArgumentsMarker', 'value': 'Oddball::kArgumentsMarker' },
    { 'name': 'OddballUndefined',       'value': 'Oddball::kUndefined' },
    { 'name': 'OddballUninitialized',   'value': 'Oddball::kUninitialized' },
    { 'name': 'OddballOther',           'value': 'Oddball::kOther' },
    { 'name': 'OddballException',       'value': 'Oddball::kException' },

    { 'name': 'prop_idx_first',
        'value': 'DescriptorArray::kFirstIndex' },
    { 'name': 'prop_kind_Data',
        'value': 'kData' },
    { 'name': 'prop_kind_Accessor',
        'value': 'kAccessor' },
    { 'name': 'prop_kind_mask',
        'value': 'PropertyDetails::KindField::kMask' },
    { 'name': 'prop_location_Descriptor',
        'value': 'kDescriptor' },
    { 'name': 'prop_location_Field',
        'value': 'kField' },
    { 'name': 'prop_location_mask',
        'value': 'PropertyDetails::LocationField::kMask' },
    { 'name': 'prop_location_shift',
        'value': 'PropertyDetails::LocationField::kShift' },
    { 'name': 'prop_attributes_NONE', 'value': 'NONE' },
    { 'name': 'prop_attributes_READ_ONLY', 'value': 'READ_ONLY' },
    { 'name': 'prop_attributes_DONT_ENUM', 'value': 'DONT_ENUM' },
    { 'name': 'prop_attributes_DONT_DELETE', 'value': 'DONT_DELETE' },
    { 'name': 'prop_attributes_mask',
        'value': 'PropertyDetails::AttributesField::kMask' },
    { 'name': 'prop_attributes_shift',
        'value': 'PropertyDetails::AttributesField::kShift' },
    { 'name': 'prop_index_mask',
        'value': 'PropertyDetails::FieldIndexField::kMask' },
    { 'name': 'prop_index_shift',
        'value': 'PropertyDetails::FieldIndexField::kShift' },
    { 'name': 'prop_representation_mask',
        'value': 'PropertyDetails::RepresentationField::kMask' },
    { 'name': 'prop_representation_shift',
        'value': 'PropertyDetails::RepresentationField::kShift' },
    { 'name': 'prop_representation_integer8',
        'value': 'Representation::Kind::kInteger8' },
    { 'name': 'prop_representation_uinteger8',
        'value': 'Representation::Kind::kUInteger8' },
    { 'name': 'prop_representation_integer16',
        'value': 'Representation::Kind::kInteger16' },
    { 'name': 'prop_representation_uinteger16',
        'value': 'Representation::Kind::kUInteger16' },
    { 'name': 'prop_representation_smi',
        'value': 'Representation::Kind::kSmi' },
    { 'name': 'prop_representation_integer32',
        'value': 'Representation::Kind::kInteger32' },
    { 'name': 'prop_representation_double',
        'value': 'Representation::Kind::kDouble' },
    { 'name': 'prop_representation_heapobject',
        'value': 'Representation::Kind::kHeapObject' },
    { 'name': 'prop_representation_tagged',
        'value': 'Representation::Kind::kTagged' },
    { 'name': 'prop_representation_external',
        'value': 'Representation::Kind::kExternal' },

    { 'name': 'prop_desc_key',
        'value': 'DescriptorArray::kEntryKeyIndex' },
    { 'name': 'prop_desc_details',
        'value': 'DescriptorArray::kEntryDetailsIndex' },
    { 'name': 'prop_desc_value',
        'value': 'DescriptorArray::kEntryValueIndex' },
    { 'name': 'prop_desc_size',
        'value': 'DescriptorArray::kEntrySize' },

    { 'name': 'elements_fast_holey_elements',
        'value': 'HOLEY_ELEMENTS' },
    { 'name': 'elements_fast_elements',
        'value': 'PACKED_ELEMENTS' },
    { 'name': 'elements_dictionary_elements',
        'value': 'DICTIONARY_ELEMENTS' },

    { 'name': 'bit_field2_elements_kind_mask',
        'value': 'Map::ElementsKindBits::kMask' },
    { 'name': 'bit_field2_elements_kind_shift',
        'value': 'Map::ElementsKindBits::kShift' },
    { 'name': 'bit_field3_dictionary_map_shift',
        'value': 'Map::DictionaryMap::kShift' },
    { 'name': 'bit_field3_number_of_own_descriptors_mask',
        'value': 'Map::NumberOfOwnDescriptorsBits::kMask' },
    { 'name': 'bit_field3_number_of_own_descriptors_shift',
        'value': 'Map::NumberOfOwnDescriptorsBits::kShift' },

    { 'name': 'off_fp_context_or_frame_type',
        'value': 'CommonFrameConstants::kContextOrFrameTypeOffset'},
    { 'name': 'off_fp_context',
        'value': 'StandardFrameConstants::kContextOffset' },
    { 'name': 'off_fp_constant_pool',
        'value': 'StandardFrameConstants::kConstantPoolOffset' },
    { 'name': 'off_fp_function',
        'value': 'JavaScriptFrameConstants::kFunctionOffset' },
    { 'name': 'off_fp_args',
        'value': 'JavaScriptFrameConstants::kLastParameterOffset' },

    { 'name': 'scopeinfo_idx_nparams',
        'value': 'ScopeInfo::kParameterCount' },
    { 'name': 'scopeinfo_idx_nstacklocals',
        'value': 'ScopeInfo::kStackLocalCount' },
    { 'name': 'scopeinfo_idx_ncontextlocals',
        'value': 'ScopeInfo::kContextLocalCount' },
    { 'name': 'scopeinfo_idx_first_vars',
        'value': 'ScopeInfo::kVariablePartIndex' },

    { 'name': 'sharedfunctioninfo_start_position_mask',
        'value': 'SharedFunctionInfo::StartPositionBits::kMask' },
    { 'name': 'sharedfunctioninfo_start_position_shift',
        'value': 'SharedFunctionInfo::StartPositionBits::kShift' },

    { 'name': 'jsarray_buffer_was_neutered_mask',
        'value': 'JSArrayBuffer::WasNeutered::kMask' },
    { 'name': 'jsarray_buffer_was_neutered_shift',
        'value': 'JSArrayBuffer::WasNeutered::kShift' },

    { 'name': 'context_idx_closure',
        'value': 'Context::CLOSURE_INDEX' },
    { 'name': 'context_idx_native',
        'value': 'Context::NATIVE_CONTEXT_INDEX' },
    { 'name': 'context_idx_prev',
        'value': 'Context::PREVIOUS_INDEX' },
    { 'name': 'context_idx_ext',
        'value': 'Context::EXTENSION_INDEX' },
    { 'name': 'context_min_slots',
        'value': 'Context::MIN_CONTEXT_SLOTS' },

    { 'name': 'namedictionaryshape_prefix_size',
        'value': 'NameDictionaryShape::kPrefixSize' },
    { 'name': 'namedictionaryshape_entry_size',
        'value': 'NameDictionaryShape::kEntrySize' },
    { 'name': 'globaldictionaryshape_entry_size',
        'value': 'GlobalDictionaryShape::kEntrySize' },

    { 'name': 'namedictionary_prefix_start_index',
        'value': 'NameDictionary::kPrefixStartIndex' },

    { 'name': 'numberdictionaryshape_prefix_size',
        'value': 'NumberDictionaryShape::kPrefixSize' },
    { 'name': 'numberdictionaryshape_entry_size',
        'value': 'NumberDictionaryShape::kEntrySize' },
];

#
# The following useful fields are missing accessors, so we define fake ones.
# Please note that extra accessors should _only_ be added to expose offsets that
# can be used to access actual V8 objects' properties. They should not be added
# for exposing other values. For instance, enumeration values or class'
# constants should be exposed by adding an entry in the "consts_misc" table, not
# in this "extras_accessors" table.
#
extras_accessors = [
    'JSFunction, context, Context, kContextOffset',
    'HeapObject, map, Map, kMapOffset',
    'JSObject, elements, Object, kElementsOffset',
    'JSObject, internal_fields, uintptr_t, kHeaderSize',
    'FixedArray, data, uintptr_t, kHeaderSize',
    'FixedTypedArrayBase, external_pointer, Object, kExternalPointerOffset',
    'JSArrayBuffer, backing_store, Object, kBackingStoreOffset',
    'JSArrayBufferView, byte_offset, Object, kByteOffsetOffset',
    'JSTypedArray, length, Object, kLengthOffset',
    'Map, instance_size_in_words, char, kInstanceSizeInWordsOffset',
    'Map, inobject_properties_start_or_constructor_function_index, char, kInObjectPropertiesStartOrConstructorFunctionIndexOffset',
    'Map, instance_type, uint16_t, kInstanceTypeOffset',
    'Map, bit_field, char, kBitFieldOffset',
    'Map, bit_field2, char, kBitField2Offset',
    'Map, bit_field3, int, kBitField3Offset',
    'Map, prototype, Object, kPrototypeOffset',
    'Oddball, kind_offset, int, kKindOffset',
    'HeapNumber, value, double, kValueOffset',
    'ConsString, first, String, kFirstOffset',
    'ConsString, second, String, kSecondOffset',
    'ExternalString, resource, Object, kResourceOffset',
    'SeqOneByteString, chars, char, kHeaderSize',
    'SeqTwoByteString, chars, char, kHeaderSize',
    'SharedFunctionInfo, code, Code, kCodeOffset',
    'SharedFunctionInfo, scope_info, ScopeInfo, kScopeInfoOffset',
    'SharedFunctionInfo, function_token_position, int, kFunctionTokenPositionOffset',
    'SharedFunctionInfo, start_position_and_type, int, kStartPositionAndTypeOffset',
    'SharedFunctionInfo, end_position, int, kEndPositionOffset',
    'SharedFunctionInfo, internal_formal_parameter_count, int, kFormalParameterCountOffset',
    'SharedFunctionInfo, compiler_hints, int, kCompilerHintsOffset',
    'SharedFunctionInfo, length, int, kLengthOffset',
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

#include "src/v8.h"
#include "src/frames.h"
#include "src/frames-inl.h" /* for architecture-specific frame constants */
#include "src/contexts.h"

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
# Get the base class
#
def get_base_class(klass):
        if (klass == 'Object'):
                return klass;

        if (not (klass in klasses)):
                return None;

        k = klasses[klass];

        return get_base_class(k['parent']);

#
# Loads class hierarchy and type information from "objects.h" etc.
#
def load_objects():
        #
        # Construct a dictionary for the classes we're sure should be present.
        #
        checktypes = {};
        for klass in expected_classes:
                checktypes[klass] = True;


        for filename in sys.argv[2:]:
                if not filename.endswith("-inl.h"):
                        load_objects_from_file(filename, checktypes)

        if (len(checktypes) > 0):
                for klass in checktypes:
                        print('error: expected class \"%s\" not found' % klass);

                sys.exit(1);


def load_objects_from_file(objfilename, checktypes):
        objfile = open(objfilename, 'r');
        in_insttype = False;

        typestr = '';

        #
        # Iterate the header file line-by-line to collect type and class
        # information. For types, we accumulate a string representing the entire
        # InstanceType enum definition and parse it later because it's easier to
        # do so without the embedded newlines.
        #
        for line in objfile:
                if (line.startswith('enum InstanceType : uint16_t {')):
                        in_insttype = True;
                        continue;

                if (in_insttype and line.startswith('};')):
                        in_insttype = False;
                        continue;

                line = re.sub('//.*', '', line.strip());

                if (in_insttype):
                        typestr += line;
                        continue;

                match = re.match('class (\w[^:]*)(: public (\w[^{]*))?\s*{\s*',
                    line);

                if (match):
                        klass = match.group(1).strip();
                        pklass = match.group(3);
                        if (pklass):
                                pklass = pklass.strip();
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
                # Cons, External, or Sliced) and an encoding (TwoByte/OneByte),
                # In the simplest case, both of these are explicit in both
                # names, as in:
                #
                #       EXTERNAL_ONE_BYTE_STRING_TYPE => ExternalOneByteString
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
                #       CONS_ONE_BYTE_STRING_TYPE => ConsString
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
                                if (cctype.find('OneByte') != -1):
                                        cctype = re.sub('OneByteString$',
                                            'SeqOneByteString', cctype);
                                else:
                                        cctype = re.sub('String$',
                                            'SeqString', cctype);

                        if (cctype.find('OneByte') == -1):
                                cctype = re.sub('String$', 'TwoByteString',
                                    cctype);

                        if (not (cctype in klasses)):
                                cctype = re.sub('OneByte', '', cctype);
                                cctype = re.sub('TwoByte', '', cctype);

                #
                # Despite all that, some types have no corresponding class.
                #
                if (cctype in klasses):
                        typeclasses[type] = cctype;
                        if (cctype in checktypes):
                                del checktypes[cctype];

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
                dtype = args[2].replace('<', '_').replace('>', '_')
                offset = args[3];

                return ({
                    'name': 'class_%s__%s__%s' % (klass, field, dtype),
                    'value': '%s::%s' % (klass, offset)
                });

        assert(kind == 'SMI_ACCESSORS' or kind == 'ACCESSORS_TO_SMI');
        klass = args[0];
        field = args[1];
        offset = args[2];

        return ({
            'name': 'class_%s__%s__%s' % (klass, field, 'SMI'),
            'value': '%s::%s' % (klass, offset)
        });

#
# Load field offset information from objects-inl.h etc.
#
def load_fields():
        for filename in sys.argv[2:]:
                if filename.endswith("-inl.h"):
                        load_fields_from_file(filename)

        for body in extras_accessors:
                fields.append(parse_field('ACCESSORS(%s)' % body));


def load_fields_from_file(filename):
        inlfile = open(filename, 'r');

        #
        # Each class's fields and the corresponding offsets are described in the
        # source by calls to macros like "ACCESSORS" (and friends).  All we do
        # here is extract these macro invocations, taking into account that they
        # may span multiple lines and may contain nested parentheses.  We also
        # call parse_field() to pick apart the invocation.
        #
        prefixes = [ 'ACCESSORS', 'ACCESSORS_GCSAFE',
                     'SMI_ACCESSORS', 'ACCESSORS_TO_SMI' ];
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
                bklass = get_base_class(klassname);
                if (bklass != 'Object'):
                        continue;
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
