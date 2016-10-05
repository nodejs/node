# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import optparse
import collections
import functools
try:
    import json
except ImportError:
    import simplejson as json

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
module_path, module_filename = os.path.split(os.path.realpath(__file__))

def read_config():
    # pylint: disable=W0703
    def json_to_object(data, output_base, config_base):
        def json_object_hook(object_dict):
            items = [(k, os.path.join(config_base, v) if k == "path" else v) for (k, v) in object_dict.items()]
            items = [(k, os.path.join(output_base, v) if k == "output" else v) for (k, v) in items]
            keys, values = zip(*items)
            return collections.namedtuple('X', keys)(*values)
        return json.loads(data, object_hook=json_object_hook)

    def init_defaults(config_tuple, path, defaults):
        keys = list(config_tuple._fields)  # pylint: disable=E1101
        values = [getattr(config_tuple, k) for k in keys]
        for i in xrange(len(keys)):
            if hasattr(values[i], "_fields"):
                values[i] = init_defaults(values[i], path + "." + keys[i], defaults)
        for optional in defaults:
            if optional.find(path + ".") != 0:
                continue
            optional_key = optional[len(path) + 1:]
            if optional_key.find(".") == -1 and optional_key not in keys:
                keys.append(optional_key)
                values.append(defaults[optional])
        return collections.namedtuple('X', keys)(*values)

    try:
        cmdline_parser = optparse.OptionParser()
        cmdline_parser.add_option("--output_base")
        cmdline_parser.add_option("--jinja_dir")
        cmdline_parser.add_option("--config")
        arg_options, _ = cmdline_parser.parse_args()
        jinja_dir = arg_options.jinja_dir
        if not jinja_dir:
            raise Exception("jinja directory must be specified")
        output_base = arg_options.output_base
        if not output_base:
            raise Exception("Base output directory must be specified")
        config_file = arg_options.config
        if not config_file:
            raise Exception("Config file name must be specified")
        config_base = os.path.dirname(config_file)
    except Exception:
        # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
        exc = sys.exc_info()[1]
        sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
        exit(1)

    try:
        config_json_file = open(config_file, "r")
        config_json_string = config_json_file.read()
        config_partial = json_to_object(config_json_string, output_base, config_base)
        config_json_file.close()
        defaults = {
            ".imported": False,
            ".imported.export_macro": "",
            ".imported.export_header": False,
            ".imported.header": False,
            ".imported.package": False,
            ".protocol.export_macro": "",
            ".protocol.export_header": False,
            ".exported": False,
            ".exported.export_macro": "",
            ".exported.export_header": False,
            ".lib": False,
            ".lib.export_macro": "",
            ".lib.export_header": False,
        }
        return (jinja_dir, config_file, init_defaults(config_partial, "", defaults))
    except Exception:
        # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
        exc = sys.exc_info()[1]
        sys.stderr.write("Failed to parse config file: %s\n\n" % exc)
        exit(1)


def to_title_case(name):
    return name[:1].upper() + name[1:]


def dash_to_camelcase(word):
    prefix = ""
    if word[0] == "-":
        prefix = "Negative"
        word = word[1:]
    return prefix + "".join(to_title_case(x) or "-" for x in word.split("-"))


def initialize_jinja_env(jinja_dir, cache_dir):
    dir = os.path.abspath(jinja_dir)
    # pylint: disable=F0401
    sys.path.insert(1, os.path.join(dir, "jinja2"))
    sys.path.insert(1, os.path.join(dir, "markupsafe"))
    import jinja2

    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(module_path),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({"to_title_case": to_title_case, "dash_to_camelcase": dash_to_camelcase})
    jinja_env.add_extension("jinja2.ext.loopcontrols")
    return jinja_env


def patch_full_qualified_refs(protocol):
    def patch_full_qualified_refs_in_domain(json, domain_name):
        if isinstance(json, list):
            for item in json:
                patch_full_qualified_refs_in_domain(item, domain_name)

        if not isinstance(json, dict):
            return
        for key in json:
            if key == "type" and json[key] == "string":
                json[key] = domain_name + ".string"
            if key != "$ref":
                patch_full_qualified_refs_in_domain(json[key], domain_name)
                continue
            if json["$ref"].find(".") == -1:
                json["$ref"] = domain_name + "." + json["$ref"]
        return

    for domain in protocol.json_api["domains"]:
        patch_full_qualified_refs_in_domain(domain, domain["domain"])


def calculate_exports(protocol):
    def calculate_exports_in_json(json_value):
        has_exports = False
        if isinstance(json_value, list):
            for item in json_value:
                has_exports = calculate_exports_in_json(item) or has_exports
        if isinstance(json_value, dict):
            has_exports = ("exported" in json_value and json_value["exported"]) or has_exports
            for key in json_value:
                has_exports = calculate_exports_in_json(json_value[key]) or has_exports
        return has_exports

    protocol.json_api["has_exports"] = False
    for domain_json in protocol.json_api["domains"]:
        domain_json["has_exports"] = calculate_exports_in_json(domain_json)
        if domain_json["has_exports"] and domain_json["domain"] in protocol.generate_domains:
            protocol.json_api["has_exports"] = True


def create_imported_type_definition(domain_name, type, imported_namespace):
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<%s::%s::API::%s>" % (imported_namespace, domain_name, type["id"]),
        "pass_type": "std::unique_ptr<%s::%s::API::%s>" % (imported_namespace, domain_name, type["id"]),
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<%s::%s::API::%s>" % (imported_namespace, domain_name, type["id"]),
        "raw_type": "%s::%s::API::%s" % (imported_namespace, domain_name, type["id"]),
        "raw_pass_type": "%s::%s::API::%s*" % (imported_namespace, domain_name, type["id"]),
        "raw_return_type": "%s::%s::API::%s*" % (imported_namespace, domain_name, type["id"]),
    }


def create_user_type_definition(domain_name, type):
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<protocol::%s::%s>" % (domain_name, type["id"]),
        "pass_type": "std::unique_ptr<protocol::%s::%s>" % (domain_name, type["id"]),
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<protocol::%s::%s>" % (domain_name, type["id"]),
        "raw_type": "protocol::%s::%s" % (domain_name, type["id"]),
        "raw_pass_type": "protocol::%s::%s*" % (domain_name, type["id"]),
        "raw_return_type": "protocol::%s::%s*" % (domain_name, type["id"]),
    }


def create_object_type_definition():
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<protocol::DictionaryValue>",
        "pass_type": "std::unique_ptr<protocol::DictionaryValue>",
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<protocol::DictionaryValue>",
        "raw_type": "protocol::DictionaryValue",
        "raw_pass_type": "protocol::DictionaryValue*",
        "raw_return_type": "protocol::DictionaryValue*",
    }


def create_any_type_definition():
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<protocol::Value>",
        "pass_type": "std::unique_ptr<protocol::Value>",
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<protocol::Value>",
        "raw_type": "protocol::Value",
        "raw_pass_type": "protocol::Value*",
        "raw_return_type": "protocol::Value*",
    }


def create_string_type_definition():
    # pylint: disable=W0622
    return {
        "return_type": "String",
        "pass_type": "const String&",
        "to_pass_type": "%s",
        "to_raw_type": "%s",
        "to_rvalue": "%s",
        "type": "String",
        "raw_type": "String",
        "raw_pass_type": "const String&",
        "raw_return_type": "String",
    }


def create_primitive_type_definition(type):
    # pylint: disable=W0622
    typedefs = {
        "number": "double",
        "integer": "int",
        "boolean": "bool"
    }
    defaults = {
        "number": "0",
        "integer": "0",
        "boolean": "false"
    }
    jsontypes = {
        "number": "TypeDouble",
        "integer": "TypeInteger",
        "boolean": "TypeBoolean",
    }
    return {
        "return_type": typedefs[type],
        "pass_type": typedefs[type],
        "to_pass_type": "%s",
        "to_raw_type": "%s",
        "to_rvalue": "%s",
        "type": typedefs[type],
        "raw_type": typedefs[type],
        "raw_pass_type": typedefs[type],
        "raw_return_type": typedefs[type],
        "default_value": defaults[type]
    }


def wrap_array_definition(type):
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<protocol::Array<%s>>" % type["raw_type"],
        "pass_type": "std::unique_ptr<protocol::Array<%s>>" % type["raw_type"],
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<protocol::Array<%s>>" % type["raw_type"],
        "raw_type": "protocol::Array<%s>" % type["raw_type"],
        "raw_pass_type": "protocol::Array<%s>*" % type["raw_type"],
        "raw_return_type": "protocol::Array<%s>*" % type["raw_type"],
        "create_type": "wrapUnique(new protocol::Array<%s>())" % type["raw_type"],
        "out_type": "protocol::Array<%s>&" % type["raw_type"],
    }


def create_type_definitions(protocol, imported_namespace):
    protocol.type_definitions = {}
    protocol.type_definitions["number"] = create_primitive_type_definition("number")
    protocol.type_definitions["integer"] = create_primitive_type_definition("integer")
    protocol.type_definitions["boolean"] = create_primitive_type_definition("boolean")
    protocol.type_definitions["object"] = create_object_type_definition()
    protocol.type_definitions["any"] = create_any_type_definition()
    for domain in protocol.json_api["domains"]:
        protocol.type_definitions[domain["domain"] + ".string"] = create_string_type_definition()
        if not ("types" in domain):
            continue
        for type in domain["types"]:
            type_name = domain["domain"] + "." + type["id"]
            if type["type"] == "object" and domain["domain"] in protocol.imported_domains:
                protocol.type_definitions[type_name] = create_imported_type_definition(domain["domain"], type, imported_namespace)
            elif type["type"] == "object":
                protocol.type_definitions[type_name] = create_user_type_definition(domain["domain"], type)
            elif type["type"] == "array":
                items_type = type["items"]["type"]
                protocol.type_definitions[type_name] = wrap_array_definition(protocol.type_definitions[items_type])
            elif type["type"] == domain["domain"] + ".string":
                protocol.type_definitions[type_name] = create_string_type_definition()
            else:
                protocol.type_definitions[type_name] = create_primitive_type_definition(type["type"])


def type_definition(protocol, name):
    return protocol.type_definitions[name]


def resolve_type(protocol, prop):
    if "$ref" in prop:
        return protocol.type_definitions[prop["$ref"]]
    if prop["type"] == "array":
        return wrap_array_definition(resolve_type(protocol, prop["items"]))
    return protocol.type_definitions[prop["type"]]


def join_arrays(dict, keys):
    result = []
    for key in keys:
        if key in dict:
            result += dict[key]
    return result


def has_disable(commands):
    for command in commands:
        if command["name"] == "disable":
            return True
    return False


def format_include(header):
    return "\"" + header + "\"" if header[0] not in "<\"" else header


def read_protocol_file(file_name, json_api):
    input_file = open(file_name, "r")
    json_string = input_file.read()
    input_file.close()
    parsed_json = json.loads(json_string)
    version = parsed_json["version"]["major"] + "." + parsed_json["version"]["minor"]
    domains = []
    for domain in parsed_json["domains"]:
        domains.append(domain["domain"])
        domain["version"] = version
    json_api["domains"] += parsed_json["domains"]
    return domains


class Protocol(object):
    def __init__(self):
        self.json_api = {}
        self.generate_domains = []
        self.imported_domains = []


def main():
    jinja_dir, config_file, config = read_config()

    protocol = Protocol()
    protocol.json_api = {"domains": []}
    protocol.generate_domains = read_protocol_file(config.protocol.path, protocol.json_api)
    protocol.imported_domains = read_protocol_file(config.imported.path, protocol.json_api) if config.imported else []
    patch_full_qualified_refs(protocol)
    calculate_exports(protocol)
    create_type_definitions(protocol, "::".join(config.imported.namespace) if config.imported else "")

    if not config.exported:
        for domain_json in protocol.json_api["domains"]:
            if domain_json["has_exports"] and domain_json["domain"] in protocol.generate_domains:
                sys.stderr.write("Domain %s is exported, but config is missing export entry\n\n" % domain_json["domain"])
                exit(1)

    if not os.path.exists(config.protocol.output):
        os.mkdir(config.protocol.output)
    if protocol.json_api["has_exports"] and not os.path.exists(config.exported.output):
        os.mkdir(config.exported.output)
    jinja_env = initialize_jinja_env(jinja_dir, config.protocol.output)

    inputs = []
    inputs.append(__file__)
    inputs.append(config_file)
    inputs.append(config.protocol.path)
    if config.imported:
        inputs.append(config.imported.path)
    templates_dir = os.path.join(module_path, "templates")
    inputs.append(os.path.join(templates_dir, "TypeBuilder_h.template"))
    inputs.append(os.path.join(templates_dir, "TypeBuilder_cpp.template"))
    inputs.append(os.path.join(templates_dir, "Exported_h.template"))
    inputs.append(os.path.join(templates_dir, "Imported_h.template"))

    h_template = jinja_env.get_template("templates/TypeBuilder_h.template")
    cpp_template = jinja_env.get_template("templates/TypeBuilder_cpp.template")
    exported_template = jinja_env.get_template("templates/Exported_h.template")
    imported_template = jinja_env.get_template("templates/Imported_h.template")

    outputs = dict()

    for domain in protocol.json_api["domains"]:
        class_name = domain["domain"]
        template_context = {
            "config": config,
            "domain": domain,
            "join_arrays": join_arrays,
            "resolve_type": functools.partial(resolve_type, protocol),
            "type_definition": functools.partial(type_definition, protocol),
            "has_disable": has_disable,
            "format_include": format_include,
        }

        if domain["domain"] in protocol.generate_domains:
            outputs[os.path.join(config.protocol.output, class_name + ".h")] = h_template.render(template_context)
            outputs[os.path.join(config.protocol.output, class_name + ".cpp")] = cpp_template.render(template_context)
            if domain["has_exports"]:
                outputs[os.path.join(config.exported.output, class_name + ".h")] = exported_template.render(template_context)
        if domain["domain"] in protocol.imported_domains and domain["has_exports"]:
            outputs[os.path.join(config.protocol.output, class_name + ".h")] = imported_template.render(template_context)

    if config.lib:
        template_context = {
            "config": config,
            "format_include": format_include,
        }

        lib_templates_dir = os.path.join(module_path, "lib")
        # Note these should be sorted in the right order.
        # TODO(dgozman): sort them programmatically based on commented includes.
        lib_h_templates = [
            "Collections_h.template",
            "ErrorSupport_h.template",
            "Values_h.template",
            "Object_h.template",
            "ValueConversions_h.template",
            "Maybe_h.template",
            "Array_h.template",
            "BackendCallback_h.template",
            "DispatcherBase_h.template",
            "Parser_h.template",
        ]

        lib_cpp_templates = [
            "Protocol_cpp.template",
            "ErrorSupport_cpp.template",
            "Values_cpp.template",
            "Object_cpp.template",
            "DispatcherBase_cpp.template",
            "Parser_cpp.template",
        ]

        forward_h_templates = [
            "Forward_h.template",
            "Allocator_h.template",
            "FrontendChannel_h.template",
        ]

        def generate_lib_file(file_name, template_files):
            parts = []
            for template_file in template_files:
                inputs.append(os.path.join(lib_templates_dir, template_file))
                template = jinja_env.get_template("lib/" + template_file)
                parts.append(template.render(template_context))
            outputs[file_name] = "\n\n".join(parts)

        generate_lib_file(os.path.join(config.lib.output, "Forward.h"), forward_h_templates)
        generate_lib_file(os.path.join(config.lib.output, "Protocol.h"), lib_h_templates)
        generate_lib_file(os.path.join(config.lib.output, "Protocol.cpp"), lib_cpp_templates)

    # Make gyp / make generatos happy, otherwise make rebuilds world.
    inputs_ts = max(map(os.path.getmtime, inputs))
    up_to_date = True
    for output_file in outputs.iterkeys():
        if not os.path.exists(output_file) or os.path.getmtime(output_file) < inputs_ts:
            up_to_date = False
            break
    if up_to_date:
        sys.exit()

    for file_name, content in outputs.iteritems():
        out_file = open(file_name, "w")
        out_file.write(content)
        out_file.close()


main()
