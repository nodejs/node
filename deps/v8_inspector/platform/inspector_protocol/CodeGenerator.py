# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys
import optparse
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
templates_dir = module_path

# In Blink, jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir))
if os.path.isdir(third_party_dir):
    sys.path.insert(1, third_party_dir)

# In Node, it is in deps folder
deps_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, "deps"))
if os.path.isdir(deps_dir):
    sys.path.insert(1, os.path.join(deps_dir, "jinja2"))
    sys.path.insert(1, os.path.join(deps_dir, "markupsafe"))

import jinja2

cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--protocol")
cmdline_parser.add_option("--include")
cmdline_parser.add_option("--string_type")
cmdline_parser.add_option("--export_macro")
cmdline_parser.add_option("--output_dir")
cmdline_parser.add_option("--output_package")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    protocol_file = arg_options.protocol
    if not protocol_file:
        raise Exception("Protocol directory must be specified")
    include_file = arg_options.include
    output_dirname = arg_options.output_dir
    if not output_dirname:
        raise Exception("Output directory must be specified")
    output_package = arg_options.output_package
    if not output_package:
        raise Exception("Output package must be specified")
    string_type = arg_options.string_type
    if not string_type:
        raise Exception("String type must be specified")
    export_macro = arg_options.export_macro
    if not export_macro:
        raise Exception("Export macro must be specified")
except Exception:
    # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
    exc = sys.exc_info()[1]
    sys.stderr.write("Failed to parse command-line arguments: %s\n\n" % exc)
    exit(1)


input_file = open(protocol_file, "r")
json_string = input_file.read()
parsed_json = json.loads(json_string)


# Make gyp / make generatos happy, otherwise make rebuilds world.
def up_to_date():
    template_ts = max(
        os.path.getmtime(__file__),
        os.path.getmtime(os.path.join(templates_dir, "TypeBuilder_h.template")),
        os.path.getmtime(os.path.join(templates_dir, "TypeBuilder_cpp.template")),
        os.path.getmtime(protocol_file))

    for domain in parsed_json["domains"]:
        name = domain["domain"]
        h_path = os.path.join(output_dirname, name + ".h")
        cpp_path = os.path.join(output_dirname, name + ".cpp")
        if not os.path.exists(h_path) or not os.path.exists(cpp_path):
            return False
        generated_ts = max(os.path.getmtime(h_path), os.path.getmtime(cpp_path))
        if generated_ts < template_ts:
            return False
    return True


if up_to_date():
    sys.exit()


def to_title_case(name):
    return name[:1].upper() + name[1:]


def dash_to_camelcase(word):
    return "".join(to_title_case(x) or "-" for x in word.split("-"))


def initialize_jinja_env(cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(templates_dir),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({"to_title_case": to_title_case, "dash_to_camelcase": dash_to_camelcase})
    jinja_env.add_extension("jinja2.ext.loopcontrols")
    return jinja_env


def output_file(file_name):
    return open(file_name, "w")


def patch_full_qualified_refs():
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

    for domain in json_api["domains"]:
        patch_full_qualified_refs_in_domain(domain, domain["domain"])


def create_user_type_definition(domain_name, type):
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


def create_string_type_definition(domain):
    return {
        "return_type": string_type,
        "pass_type": ("const %s&" % string_type),
        "to_pass_type": "%s",
        "to_raw_type": "%s",
        "to_rvalue": "%s",
        "type": string_type,
        "raw_type": string_type,
        "raw_pass_type": ("const %s&" % string_type),
        "raw_return_type": string_type,
    }


def create_primitive_type_definition(type):
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
        "number": "TypeNumber",
        "integer": "TypeNumber",
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


type_definitions = {}
type_definitions["number"] = create_primitive_type_definition("number")
type_definitions["integer"] = create_primitive_type_definition("integer")
type_definitions["boolean"] = create_primitive_type_definition("boolean")
type_definitions["object"] = create_object_type_definition()
type_definitions["any"] = create_any_type_definition()


def wrap_array_definition(type):
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


def create_type_definitions():
    for domain in json_api["domains"]:
        type_definitions[domain["domain"] + ".string"] = create_string_type_definition(domain["domain"])
        if not ("types" in domain):
            continue
        for type in domain["types"]:
            if type["type"] == "object":
                type_definitions[domain["domain"] + "." + type["id"]] = create_user_type_definition(domain["domain"], type)
            elif type["type"] == "array":
                items_type = type["items"]["type"]
                type_definitions[domain["domain"] + "." + type["id"]] = wrap_array_definition(type_definitions[items_type])
            elif type["type"] == domain["domain"] + ".string":
                type_definitions[domain["domain"] + "." + type["id"]] = create_string_type_definition(domain["domain"])
            else:
                type_definitions[domain["domain"] + "." + type["id"]] = create_primitive_type_definition(type["type"])


def type_definition(name):
    return type_definitions[name]


def resolve_type(property):
    if "$ref" in property:
        return type_definitions[property["$ref"]]
    if property["type"] == "array":
        return wrap_array_definition(resolve_type(property["items"]))
    return type_definitions[property["type"]]


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


generate_domains = []
json_api = {}
json_api["domains"] = parsed_json["domains"]

for domain in parsed_json["domains"]:
    generate_domains.append(domain["domain"])

if include_file:
    input_file = open(include_file, "r")
    json_string = input_file.read()
    parsed_json = json.loads(json_string)
    json_api["domains"] += parsed_json["domains"]


patch_full_qualified_refs()
create_type_definitions()

if not os.path.exists(output_dirname):
    os.mkdir(output_dirname)
jinja_env = initialize_jinja_env(output_dirname)

h_template_name = "/TypeBuilder_h.template"
cpp_template_name = "/TypeBuilder_cpp.template"
h_template = jinja_env.get_template(h_template_name)
cpp_template = jinja_env.get_template(cpp_template_name)


def generate(domain):
    class_name = domain["domain"]
    h_file_name = output_dirname + "/" + class_name + ".h"
    cpp_file_name = output_dirname + "/" + class_name + ".cpp"

    template_context = {
        "domain": domain,
        "join_arrays": join_arrays,
        "resolve_type": resolve_type,
        "type_definition": type_definition,
        "has_disable": has_disable,
        "export_macro": export_macro,
        "output_package": output_package,
    }
    h_file = output_file(h_file_name)
    cpp_file = output_file(cpp_file_name)
    h_file.write(h_template.render(template_context))
    cpp_file.write(cpp_template.render(template_context))
    h_file.close()
    cpp_file.close()


for domain in json_api["domains"]:
    if domain["domain"] in generate_domains:
        generate(domain)
