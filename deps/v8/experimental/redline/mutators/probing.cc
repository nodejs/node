// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/mutators/probing.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "experimental/redline/base/json_parser.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/generators/codegen.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

// Reports each missing property an object is asked for, once, as
// "PROBE_HIT: {"id": ..., "kind": "get"|"set", "key": ...}". Well-known
// symbols are reported by their description ("Symbol.toPrimitive").
constexpr char kProbeRuntime[] = R"js(const __redline_probe = (function() {
  const ReflectGet = Reflect.get;
  const ReflectSet = Reflect.set;
  const ReflectHas = Reflect.has;
  const ReflectApply = Reflect.apply;
  const ObjectGetPrototypeOf = Object.getPrototypeOf;
  const ObjectSetPrototypeOf = Object.setPrototypeOf;
  const ProxyConstructor = Proxy;
  const JSONStringify = JSON.stringify;
  const emit = typeof redline === 'function' ? function(s) { redline('REDLINE_PRINT', s); }
             : typeof fuzzilli === 'function' ? function(s) { fuzzilli('FUZZILLI_PRINT', s); }
                                              : function(s) {};
  const seen = {__proto__: null};
  let reporting = false;
  function record(id, kind, target, key) {
    if (reporting) return;
    reporting = true;
    try {
      if (ReflectHas(target, key) &&
          (kind !== 'get' ||
           (key !== 'valueOf' && key !== 'toString' && key !== 'constructor'))) {
        return;
      }
      let name = key;
      if (typeof key === 'symbol') {
        const d = key.description;
        if (typeof d !== 'string' || d.length < 8 || d.substring(0, 7) !== 'Symbol.') return;
        name = d;
      } else if (typeof key !== 'string' || key.length === 0 || key.length > 40) {
        return;
      }
      const token = id + '|' + kind + '|' + name;
      if (seen[token]) return;
      seen[token] = true;
      emit('PROBE_HIT: ' + JSONStringify({id: id, kind: kind, key: name}));
    } catch (e) {
    } finally {
      reporting = false;
    }
  }
  // A probe in a loop body runs again for the same object: one proxy only.
  const probed = new WeakMap();
  const WeakMapHas = WeakMap.prototype.has;
  const WeakMapSet = WeakMap.prototype.set;
  function probe(id, v) {
    if (v === null || (typeof v !== 'object' && typeof v !== 'function')) return;
    if (ReflectApply(WeakMapHas, probed, [v])) return;
    ReflectApply(WeakMapSet, probed, [v, true]);
    let proto, newProto;
    const handler = {
      get(target, key, receiver) {
        if (key === '__proto__' && receiver === v) return proto;
        if (receiver === newProto) return ReflectGet(target, key);
        record(id, 'get', target, key);
        return ReflectGet(target, key, receiver);
      },
      set(target, key, value, receiver) {
        if (receiver === newProto) return ReflectSet(target, key, value);
        record(id, 'set', target, key);
        return ReflectSet(target, key, value, receiver);
      },
      has(target, key) {
        record(id, 'get', target, key);
        return ReflectHas(target, key);
      },
    };
    try {
      proto = ObjectGetPrototypeOf(v);
      newProto = new ProxyConstructor(proto === null ? {__proto__: null} : proto, handler);
      ObjectSetPrototypeOf(v, newProto);
    } catch (e) {}
  }
  return probe;
})();
try { %NeverOptimizeFunction(__redline_probe); } catch (e) {}
)js";

bool ProbeWorthy(const Variable& v) {
  switch (v.type) {
    case JsType::kNumber:
    case JsType::kString:
    case JsType::kBoolean:
    case JsType::kBigInt:
    case JsType::kSymbol:
    case JsType::kNullish:
      return false;
    default:
      return v.name != "arguments" && v.name != "undefined";
  }
}

struct Hit {
  std::string kind;  // "get" or "set"
  std::string key;
};

// `v.key`, `v[0]`, `v["key"]` or `v[Symbol.name]`.
Expression* Target(Tree* t, std::string_view variable, const std::string& key) {
  return RuntimeAssistedMutator::MemberAccess(t, mk::Id(t, variable), key);
}

Expression* KeyExpression(Tree* t, const std::string& key) {
  return RuntimeAssistedMutator::KeyLiteral(t, key);
}

// A function suitable as the value of a well-known symbol property.
Expression* SymbolHook(CodeGen& cg, Tree* t, const std::string& key) {
  std::vector<Statement*> body;
  if (key == "Symbol.iterator" || key == "Symbol.asyncIterator") {
    // function*() { yield <value>; yield <value>; }
    body.push_back(
        mk::ExprStmt(t, mk::Yield(t, cg.Value(JsType::kUnknown, 1))));
    body.push_back(
        mk::ExprStmt(t, mk::Yield(t, cg.Value(JsType::kUnknown, 1))));
    return mk::Function(t, "", {}, std::move(body),
                        key == "Symbol.asyncIterator",
                        /*is_generator=*/true);
  }
  if (key == "Symbol.hasInstance") {
    body.push_back(mk::Return(t, cg.Boolean()));
    return mk::Function(t, "", {"v"}, std::move(body));
  }
  if (key == "Symbol.species") {
    return cg.Callable();
  }
  if (key == "Symbol.toStringTag") {
    return cg.String();
  }
  if (key == "Symbol.isConcatSpreadable") {
    return cg.Boolean();
  }
  // Symbol.toPrimitive and the string/regexp hooks: a function returning a
  // value.
  body.push_back(mk::Return(t, cg.Value(JsType::kUnknown, 1)));
  return mk::Function(t, "", {"hint"}, std::move(body));
}

}  // namespace

ProbingMutator::ProbingMutator()
    : RuntimeAssistedMutator("Probing",
                             "finds which missing properties a program asks an "
                             "object for and installs them",
                             2, false) {}

std::string_view ProbingMutator::runtime() const { return kProbeRuntime; }

REDLINE_TUNABLE(kProbingMaxProbes, "mutators.Probing.max_probes", 10, 1, 32,
                "most variables probed per application");
REDLINE_TUNABLE(kProbingMaxLoopProbes, "mutators.Probing.max_loop_probes", 4, 0,
                32, "most probes in loop bodies per application");
REDLINE_TUNABLE(kProbingChance, "mutators.Probing.chance", 0.25, 0, 1,
                "probability that a variable the program references is probed "
                "at its definition");
REDLINE_TUNABLE(kProbingUnreferencedCandidateChance,
                "mutators.Probing.unreferenced_candidate_chance", 0.1, 0, 1,
                "probability that a variable the program never references is "
                "still a probe candidate");

bool ProbingMutator::Mutate(Builder& b, MutationContext& ctx) {
  Random& r = b.rng();
  ctx.probes.clear();
  // A probe only learns something if the program goes on to use the
  // object. It sits right after the definition, before the uses.
  std::vector<DefinitionSite> sites = DefinitionSites(b);
  std::vector<double> chances;
  chances.reserve(sites.size());
  for (const DefinitionSite& site : sites) {
    const bool used = !site.variable.binding->references.empty() ||
                      r.Chance(kProbingUnreferencedCandidateChance());
    chances.push_back(ProbeWorthy(site.variable) && used ? kProbingChance()
                                                         : 0.0);
  }
  std::vector<size_t> picked =
      PickSites(r, sites, chances, kProbingMaxProbes.AsInt(),
                kProbingMaxLoopProbes.AsInt());
  if (picked.empty()) {
    return false;
  }
  const unsigned first_id = static_cast<unsigned>(r.Next() % 100000);
  std::vector<std::pair<Builder::Slot, Statement*>> insertions;
  for (size_t index : picked) {
    ExploreProbe probe;
    probe.id = StringPrintf(
        "p%u", first_id + static_cast<unsigned>(ctx.probes.size()));
    probe.variable = std::string(sites[index].variable.name);
    insertions.emplace_back(sites[index].slot,
                            MakeProbe(b.tree(), kProbeFunction, probe, {}));
    ctx.probes.push_back(std::move(probe));
  }
  InsertProbes(b, &insertions);
  ctx.detail = "probing";
  for (const ExploreProbe& p : ctx.probes) {
    ctx.detail += " " + p.variable;
  }
  return true;
}

REDLINE_TUNABLE(kProbingSecondInstallChance,
                "mutators.Probing.second_install_chance", 0.4, 0, 1,
                "probability of installing a second missing property when "
                "several were asked for");
REDLINE_TUNABLE(kProbingAccessorChance, "mutators.Probing.accessor_chance",
                0.35, 0, 1,
                "probability of installing a missing property as a "
                "getter/setter rather than a plain value");
REDLINE_TUNABLE(kProbingReuseFunctionChance,
                "mutators.Probing.reuse_function_chance", 0.5, 0, 1,
                "probability that a callable property gets a visible function "
                "variable rather than a new function");
REDLINE_TUNABLE(kProbingMethodMaxStatements,
                "mutators.Probing.method_max_statements", 2, 0, 16,
                "most generated statements in a new callable property");

// Whether the engine or a builtin expects a function when it reads `key`.
bool IsCallableProperty(std::string_view key) {
  return key == "valueOf" || key == "toString" || key == "constructor" ||
         key == "then" || key == "next" || key == "get" || key == "set";
}

Expression* SelectPropertyValue(GenContext& gen, CodeGen& cg, Tree* t,
                                const std::string& key) {
  if (!IsCallableProperty(key)) {
    return cg.Value(JsType::kUnknown, 1);
  }
  Random& r = gen.rng();
  if (r.Chance(kProbingReuseFunctionChance())) {
    if (Identifier* fn = cg.Variable(
            key == "constructor" ? JsType::kClass : JsType::kFunction)) {
      return fn;
    }
  }
  GenContext nested = gen.Nested(true, false, false, false);
  CodeGen icg(nested);
  std::vector<Statement*> body =
      icg.Body(nested, r.SmallCount(kProbingMethodMaxStatements.AsInt()));
  body.push_back(mk::Return(t, icg.Value(JsType::kUnknown, 1)));
  return mk::Function(t, "", {}, std::move(body));
}

int ProbingMutator::Apply(Builder& b, const std::vector<ExploreProbe>& probes,
                          const std::string& fuzzout, MutationContext& ctx,
                          std::string* detail) {
  std::map<std::string, std::vector<Hit>> hits;
  size_t start = 0;
  while (start < fuzzout.size()) {
    size_t nl = fuzzout.find('\n', start);
    if (nl == std::string::npos) {
      nl = fuzzout.size();
    }
    std::string_view line(fuzzout.data() + start, nl - start);
    start = nl + 1;
    constexpr std::string_view kHit = "PROBE_HIT: ";
    if (!line.starts_with(kHit)) {
      continue;
    }
    JsonValue json;
    std::string error;
    if (!JsonValue::Parse(line.substr(kHit.size()), &json, &error)) {
      continue;
    }
    if (!json.is_object()) {
      continue;
    }
    std::string id = json.GetString("id", "");
    Hit hit{json.GetString("kind", "get"), json.GetString("key", "")};
    if (id.empty() || hit.key.empty()) {
      continue;
    }
    hits[id].push_back(std::move(hit));
  }
  Random& r = b.rng();
  Tree* t = b.tree();
  int applied = 0;
  std::string description;
  for (const ExploreProbe& probe : probes) {
    Statement* statement = FindProbe(b, kProbeFunction, probe.id);
    if (statement == nullptr) {
      continue;
    }
    Builder::Slot slot = b.SlotOf(statement);
    if (!slot.valid()) {
      continue;
    }
    std::vector<Statement*> replacement;
    auto it = hits.find(probe.id);
    if (it != hits.end() && !it->second.empty()) {
      GenContext gen = MakeGenContext(b, slot, *ctx.generators, *ctx.env);
      CodeGen cg(gen);
      std::vector<Hit>& found = it->second;
      int install =
          1 +
          (found.size() > 1 && r.Chance(kProbingSecondInstallChance()) ? 1 : 0);
      std::set<std::string> done;
      for (int i = 0; i < install; i++) {
        const Hit& hit = found[r.Index(found.size())];
        if (!done.insert(hit.key).second) {
          continue;
        }
        if (hit.key.starts_with("Symbol.")) {
          replacement.push_back(
              mk::ExprStmt(t, mk::Assign(t, Target(t, probe.variable, hit.key),
                                         SymbolHook(cg, t, hit.key))));
        } else if (r.Chance(kProbingAccessorChance())) {
          // Object.defineProperty(v, "key", {get() {...}, set(x) {...},
          // configurable: true})
          std::vector<Statement*> getter_body = {
              mk::Return(t, SelectPropertyValue(gen, cg, t, hit.key))};
          std::vector<Node*> descriptor;
          descriptor.push_back(mk::Method(
              t, "get", mk::Function(t, "", {}, std::move(getter_body))));
          if (hit.kind == "set" || r.Coin()) {
            std::string_view param = b.FreshName("x");
            std::vector<Statement*> setter_body;
            Expression* sink = cg.Target();
            if (sink != nullptr) {
              setter_body.push_back(
                  mk::ExprStmt(t, mk::Assign(t, sink, mk::Id(t, param))));
            }
            descriptor.push_back(mk::Method(
                t, "set",
                mk::Function(t, "", {param}, std::move(setter_body))));
          }
          descriptor.push_back(mk::Prop(t, "configurable", mk::Bool(t, true)));
          descriptor.push_back(
              mk::Prop(t, "enumerable", mk::Bool(t, r.Coin())));
          replacement.push_back(mk::ExprStmt(
              t, mk::CallMethod(
                     t, mk::Id(t, "Object"), "defineProperty",
                     {mk::Id(t, probe.variable), KeyExpression(t, hit.key),
                      mk::Object(t, std::move(descriptor))})));
        } else {
          replacement.push_back(mk::ExprStmt(
              t, mk::Assign(t, Target(t, probe.variable, hit.key),
                            SelectPropertyValue(gen, cg, t, hit.key))));
        }
        if (!description.empty()) {
          description += "; ";
        }
        description += probe.variable + " " + hit.kind + " " + hit.key;
      }
      if (!replacement.empty()) {
        applied++;
      }
    }
    b.Remove(slot);
    if (!replacement.empty()) {
      b.InsertAll(slot, replacement);
    }
  }
  if (detail != nullptr) {
    *detail = "installed " + description;
  }
  return applied;
}

}  // namespace redline
