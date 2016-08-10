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
blink_third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir,
    "third_party"))
if os.path.isdir(blink_third_party_dir):
    sys.path.insert(1, blink_third_party_dir)

# In V8, it is in third_party folder
v8_third_party_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, "third_party"))

if os.path.isdir(v8_third_party_dir):
    sys.path.insert(1, v8_third_party_dir)

# In Node, it is in deps folder
deps_dir = os.path.normpath(os.path.join(
    module_path, os.pardir, os.pardir, os.pardir, os.pardir, "third_party"))

if os.path.isdir(deps_dir):
    sys.path.insert(1, os.path.join(deps_dir, "jinja2"))
    sys.path.insert(1, os.path.join(deps_dir, "markupsafe"))

import jinja2

cmdline_parser = optparse.OptionParser()
cmdline_parser.add_option("--protocol")
cmdline_parser.add_option("--include")
cmdline_parser.add_option("--include_package")
cmdline_parser.add_option("--string_type")
cmdline_parser.add_option("--export_macro")
cmdline_parser.add_option("--output_dir")
cmdline_parser.add_option("--output_package")
cmdline_parser.add_option("--exported_dir")
cmdline_parser.add_option("--exported_package")

try:
    arg_options, arg_values = cmdline_parser.parse_args()
    protocol_file = arg_options.protocol
    if not protocol_file:
        raise Exception("Protocol directory must be specified")
    include_file = arg_options.include
    include_package = arg_options.include_package
    if include_file and not include_package:
        raise Exception("Include package must be specified when using include file")
    if include_package and not include_file:
        raise Exception("Include file must be specified when using include package")
    output_dirname = arg_options.output_dir
    if not output_dirname:
        raise Exception("Output directory must be specified")
    output_package = arg_options.output_package
    if not output_package:
        raise Exception("Output package must be specified")
    exported_dirname = arg_options.exported_dir
    if not exported_dirname:
        exported_dirname = os.path.join(output_dirname, "exported")
    exported_package = arg_options.exported_package
    if not exported_package:
        exported_package = os.path.join(output_package, "exported")
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
        os.path.getmtime(os.path.join(templates_dir, "Exported_h.template")),
        os.path.getmtime(os.path.join(templates_dir, "Imported_h.template")),
        os.path.getmtime(protocol_file))

    for domain in parsed_json["domains"]:
        name = domain["domain"]
        paths = []
        if name in generate_domains:
            paths = [os.path.join(output_dirname, name + ".h"), os.path.join(output_dirname, name + ".cpp")]
            if domain["has_exports"]:
                paths.append(os.path.join(exported_dirname, name + ".h"))
        if name in include_domains and domain["has_exports"]:
            paths = [os.path.join(output_dirname, name + '.h')]
        for path in paths:
            if not os.path.exists(path):
                return False
            generated_ts = os.path.getmtime(path)
            if generated_ts < template_ts:
                return False
    return True


def to_title_case(name):
    return name[:1].upper() + name[1:]


def dash_to_camelcase(word):
    prefix = ""
    if word[0] == "-":
        prefix = "Negative"
        word = word[1:]
    return prefix + "".join(to_title_case(x) or "-" for x in word.split("-"))


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
        return

    for domain in json_api["domains"]:
        patch_full_qualified_refs_in_domain(domain, domain["domain"])


def calculate_exports():
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

    json_api["has_exports"] = False
    for domain_json in json_api["domains"]:
        domain_json["has_exports"] = calculate_exports_in_json(domain_json)
        json_api["has_exports"] = json_api["has_exports"] or domain_json["has_exports"]


def create_include_type_definition(domain_name, type):
    # pylint: disable=W0622
    return {
        "return_type": "std::unique_ptr<protocol::%s::API::%s>" % (domain_name, type["id"]),
        "pass_type": "std::unique_ptr<protocol::%s::API::%s>" % (domain_name, type["id"]),
        "to_raw_type": "%s.get()",
        "to_pass_type": "std::move(%s)",
        "to_rvalue": "std::move(%s)",
        "type": "std::unique_ptr<protocol::%s::API::%s>" % (domain_name, type["id"]),
        "raw_type": "protocol::%s::API::%s" % (domain_name, type["id"]),
        "raw_pass_type": "protocol::%s::API::%s*" % (domain_name, type["id"]),
        "raw_return_type": "protocol::%s::API::%s*" % (domain_name, type["id"]),
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


def create_string_type_definition(domain):
    # pylint: disable=W0622
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


type_definitions = {}
type_definitions["number"] = create_primitive_type_definition("number")
type_definitions["integer"] = create_primitive_type_definition("integer")
type_definitions["boolean"] = create_primitive_type_definition("boolean")
type_definitions["object"] = create_object_type_definition()
type_definitions["any"] = create_any_type_definition()


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


def create_type_definitions():
    for domain in json_api["domains"]:
        type_definitions[domain["domain"] + ".string"] = create_string_type_definition(domain["domain"])
        if not ("types" in domain):
            continue
        for type in domain["types"]:
            type_name = domain["domain"] + "." + type["id"]
            if type["type"] == "object" and domain["domain"] in include_domains:
                type_definitions[type_name] = create_include_type_definition(domain["domain"], type)
            elif type["type"] == "object":
                type_definitions[type_name] = create_user_type_definition(domain["domain"], type)
            elif type["type"] == "array":
                items_type = type["items"]["type"]
                type_definitions[type_name] = wrap_array_definition(type_definitions[items_type])
            elif type["type"] == domain["domain"] + ".string":
                type_definitions[type_name] = create_string_type_definition(domain["domain"])
            else:
                type_definitions[type_name] = create_primitive_type_definition(type["type"])


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


def generate(domain_object, template, file_name):
    template_context = {
        "domain": domain_object,
        "join_arrays": join_arrays,
        "resolve_type": resolve_type,
        "type_definition": type_definition,
        "has_disable": has_disable,
        "export_macro": export_macro,
        "output_package": output_package,
        "exported_package": exported_package,
        "include_package": include_package
    }
    out_file = output_file(file_name)
    out_file.write(template.render(template_context))
    out_file.close()


generate_domains = []
include_domains = []
json_api = {}
json_api["domains"] = parsed_json["domains"]

for domain in parsed_json["domains"]:
    generate_domains.append(domain["domain"])

if include_file:
    input_file = open(include_file, "r")
    json_string = input_file.read()
    parsed_json = json.loads(json_string)
    for domain in parsed_json["domains"]:
        include_domains.append(domain["domain"])
    json_api["domains"] += parsed_json["domains"]

patch_full_qualified_refs()
calculate_exports()
create_type_definitions()

if up_to_date():
    sys.exit()
if not os.path.exists(output_dirname):
    os.mkdir(output_dirname)
if json_api["has_exports"] and not os.path.exists(exported_dirname):
    os.mkdir(exported_dirname)

jinja_env = initialize_jinja_env(output_dirname)
h_template = jinja_env.get_template("/TypeBuilder_h.template")
cpp_template = jinja_env.get_template("/TypeBuilder_cpp.template")
exported_template = jinja_env.get_template("/Exported_h.template")
imported_template = jinja_env.get_template("/Imported_h.template")

for domain in json_api["domains"]:
    class_name = domain["domain"]
    if domain["domain"] in generate_domains:
        generate(domain, h_template, output_dirname + "/" + class_name + ".h")
        generate(domain, cpp_template, output_dirname + "/" + class_name + ".cpp")
        if domain["has_exports"]:
            generate(domain, exported_template, exported_dirname + "/" + class_name + ".h")
    if domain["domain"] in include_domains and domain["has_exports"]:
        generate(domain, imported_template, output_dirname + "/" + class_name + ".h")
