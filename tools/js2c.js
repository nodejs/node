#!/usr/bin/env node

'use strict';

const { readFileSync, writeFileSync } = require('fs');
const path = require('path');

const [output, ...inputs] = process.argv.slice(2);

const TEMPLATE = `\
#include "node.h"
#include "node_javascript.h"
#include "v8.h"
#include "env.h"
#include "env-inl.h"
namespace node {
{definitions}
v8::Local<v8::String> LoadersBootstrapperSource(Environment* env) {
  return internal_bootstrap_loaders_value.ToStringChecked(env->isolate());
}
v8::Local<v8::String> NodeBootstrapperSource(Environment* env) {
  return internal_bootstrap_node_value.ToStringChecked(env->isolate());
}
void DefineJavaScript(Environment* env, v8::Local<v8::Object> target) {
  {initializers}
}
}  // namespace node
`;

const ONE_BYTE_STRING = `
static const uint8_t raw_{name}[] = { {data} };
static struct : public v8::String::ExternalOneByteStringResource {
  const char* data() const override {
    return reinterpret_cast<const char*>(raw_{name});
  }
  size_t length() const override { return arraysize(raw_{name}); }
  void Dispose() override { /* Default calls \`delete this\`. */ }
  v8::Local<v8::String> ToStringChecked(v8::Isolate* isolate) {
    return v8::String::NewExternalOneByte(isolate, this).ToLocalChecked();
  }
} {name};
`;

const TWO_BYTE_STRING = `
static const uint16_t raw_{name}[] = { {data} };
static struct : public v8::String::ExternalStringResource {
  const uint16_t* data() const override { return raw_{name}; }
  size_t length() const override { return arraysize(raw_{name}); }
  void Dispose() override { /* Default calls \`delete this\`. */ }
  v8::Local<v8::String> ToStringChecked(v8::Isolate* isolate) {
    return v8::String::NewExternalTwoByte(isolate, this).ToLocalChecked();
  }
} {name};
`;

const INITIALIZER = `\
CHECK(target->Set(env->context(),
                  {key}.ToStringChecked(env->isolate()),
                  {value}.ToStringChecked(env->isolate())).FromJust());
`;

const DEPRECATED_DEPS = `\
'use strict';
process.emitWarning(
  'Requiring Node.js-bundled \\'{module}\\' module is deprecated. Please ' +
  'install the necessary module locally.', 'DeprecationWarning', 'DEP0084');
module.exports = require('internal/deps/{module}');
`;


const INLINE_MACRO_PATTERN = /^macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\s*=\s*([^;]*);$/mg;
const MACRO_PATTERN = /^macro\s+([a-zA-Z0-9_]+)\s*\(([^)]*)\)\n(.*)\nendmacro$/mg;

const format = (template, args) => {
  for (const name in args)
    template = template.replace(new RegExp(`\\{${name}\\}`, 'g'), args[name]);
  return template;
};

const macros = {};
const definitions = [];
const initializers = [];

const utf16be = (str) => {
  const buffer = Buffer.from(str, 'UCS-2');
  const l = buffer.length;
  if (l & 0x01)
    throw new Error('uneven buffer length');
  for (var i = 0; i < l; i += 2) {
    const e = buffer[i];
    buffer[i] = buffer[i + 1];
    buffer[i + 1] = e;
  }
  return buffer;
};

const expandMacros = (source) => {
  const lines = source.split('\n');
  for (const [k, macro] of Object.entries(macros)
    .sort(([n1], [n2]) => n2.length - n1.length)) {
    for (var i = 0; i < lines.length; i++) {
      const line = lines[i];
      const index = line.indexOf(`${k}(`);
      if (index === -1)
        continue;

      let level = 1;
      let end = index + k.length + 1;
      const args = [''];
      let argIndex = 0;
      while (end < line.length && level > 0) {
        const char = line[end];
        if (['(', '[', '{'].includes(char)) {
          level++;
          if (level > 1)
            args[argIndex] += char;
        } else if ([')', '[', '}'].includes(char)) {
          level--;
          if (level > 1)
            args[argIndex] += char;
        } else if (char === ',') {
          args[++argIndex] = '';
        } else {
          args[argIndex] += char;
        }
        end++;
      }
      let body = macro.body;
      for (var j = 0; j < args.length; j++)
        body = body.replace(new RegExp(macro.args[j], 'g'), args[j].trim());

      lines[i] = line.slice(0, index) +
        expandMacros(body) +
        line.slice(end, line.length);
    }
  }
  return lines.join('\n');
};

const render = (name, source) => {
  let template;
  let data = '';
  source = Buffer.from(source);
  if (source.some((c) => c > 127)) {
    template = TWO_BYTE_STRING;
    const utf16 = utf16be(source.toString());
    for (let i = 0; i < utf16.length; i += 2)
      data += `${utf16[i] * 256 + (utf16[i + 1] || 0)},`;
  } else {
    template = ONE_BYTE_STRING;
    for (let i = 0; i < source.length; i += 1)
      data += `${source[i]},`;
  }

  return format(template, { name, data });
};

const queue = [];

for (const input of inputs) {
  const source = readFileSync(path.resolve(process.cwd(), input), 'utf8');

  if (/\.py$/.test(input)) {
    let match;

    INLINE_MACRO_PATTERN.lastIndex = 0;
    MACRO_PATTERN.lastIndex = 0;
    while (match = (INLINE_MACRO_PATTERN.exec(source) ||
           MACRO_PATTERN.exec(source))) {
      const [, name, args, body] = match;
      macros[name] = { args: args.split(/, ?/), body };
    }
  } else {
    queue.push({ input, source });
  }
}

for (let { source, input } of queue) { // eslint-disable-line prefer-const
  let deprecatedDeps;

  let name = input;
  if (/\/|\\\\/.test(name)) {
    let split = name.split(/\/|\\\\/);
    if (split[0] === 'deps') {
      if (split[1] === 'node-inspect' || split[1] === 'v8')
        deprecatedDeps = split.slice(1);
      split = ['internal', ...split];
    } else {
      split = split.slice(1);
    }
    name = split.join('/');
  }

  if (/\.gypi$/.test(name)) {
    source = source
      .replace(/#.*?\n/g, '')
      .replace(/'/g, '"');
  }

  name = name.split('.')[0];

  const varName = name.replace(/[^a-zA-Z0-9]/g, '_');
  const key = `${varName}_key`;
  const value = `${varName}_value`;

  definitions.push(render(key, name));
  definitions.push(render(value, expandMacros(source)));
  initializers.push(format(INITIALIZER, { key, value }));

  if (deprecatedDeps) {
    name = deprecatedDeps.join('/');
    name = name.split('.')[0];
    const varName = name.replace(/[^a-zA-Z0-9]/g, '_');
    const key = `${varName}_key`;
    const value = `${varName}_value`;
    definitions.push(render(key, name));
    definitions.push(
      render(value, format(DEPRECATED_DEPS, { module: name })));
    initializers.push(format(INITIALIZER, { key, value }));
  }
}

const final = format(TEMPLATE, {
  definitions: definitions.join(''),
  initializers: initializers.join(''),
});

writeFileSync(path.resolve(process.cwd(), output), final);
