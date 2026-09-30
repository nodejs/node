// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "src/torque/implementation-visitor.h"

namespace v8::internal::torque {

namespace {

// Contains all necessary state for a single class type during the process of
// assigning instance types, and provides a convenient way to access the list of
// types that inherit from this one.
struct InstanceTypeTree {
  explicit InstanceTypeTree(const ClassType* type)
      : type(type),
        start(INT_MAX),
        end(INT_MIN),
        value(-1),
        num_values(0),
        num_own_values(0) {}
  const ClassType* type;
  std::vector<std::unique_ptr<InstanceTypeTree>> children;
  int start;  // Start of range for this and subclasses, or INT_MAX.
  int end;    // End of range for this and subclasses, or INT_MIN.
  int value;  // Assigned value for this class itself, or -1 when unassigned.
  int num_values;      // Number of values assigned for this and subclasses.
  int num_own_values;  // How many values this needs (not including subclasses).
};

// Assembles all class types into a tree, but doesn't yet attempt to assign
// instance types for them.
std::unique_ptr<InstanceTypeTree> BuildInstanceTypeTree() {
  // First, build InstanceTypeTree instances for every class but don't try to
  // attach them to their subclasses yet.
  std::unordered_map<const ClassType*, InstanceTypeTree*> map_by_type;
  std::vector<std::unique_ptr<InstanceTypeTree>> unparented_types;
  for (auto& p : GlobalContext::AllDeclarables()) {
    if (const TypeAlias* alias = TypeAlias::DynamicCast(p.get())) {
      const Type* type = alias->type();
      const ClassType* class_type = ClassType::DynamicCast(type);
      if (class_type == nullptr) {
        continue;
      }
      auto& map_slot = map_by_type[class_type];
      if (map_slot != nullptr) {
        continue;  // We already encountered this type.
      }
      std::unique_ptr<InstanceTypeTree> type_tree =
          std::make_unique<InstanceTypeTree>(class_type);
      map_slot = type_tree.get();
      unparented_types.push_back(std::move(type_tree));
    }
  }

  // Second, assemble them all into a tree following the inheritance hierarchy.
  std::unique_ptr<InstanceTypeTree> root;
  for (auto& type_tree : unparented_types) {
    const ClassType* parent = type_tree->type->GetSuperClass();
    if (parent == nullptr) {
      if (root != nullptr) {
        Error("Expected only one root class type. Found: ", root->type->name(),
              " and ", type_tree->type->name())
            .Position(type_tree->type->GetPosition());
      }
      root = std::move(type_tree);
    } else {
      map_by_type[parent]->children.push_back(std::move(type_tree));
    }
  }
  return root;
}

// Propagates constraints about instance types from children to their parents.
void PropagateInstanceTypeConstraints(InstanceTypeTree* root) {
  for (auto& child : root->children) {
    PropagateInstanceTypeConstraints(child.get());
    if (child->start < root->start) root->start = child->start;
    if (child->end > root->end) root->end = child->end;
    root->num_values += child->num_values;
  }
  const InstanceTypeConstraints& constraints =
      root->type->GetInstanceTypeConstraints();
  if (!root->type->IsAbstract() && !root->type->HasSameInstanceTypeAsParent()) {
    root->num_own_values = 1;
  }
  root->num_values += root->num_own_values;
  if (constraints.num_flags_bits != -1) {
    // Children won't get any types assigned; must be done manually in C++.
    root->children.clear();
    root->num_values = 1 << constraints.num_flags_bits;
    root->num_own_values = root->num_values;
    root->start = 0;
    root->end = root->num_values - 1;
  }
  if (constraints.value != -1) {
    if (root->num_own_values != 1) {
      Error("Instance type value requested for abstract class ",
            root->type->name())
          .Position(root->type->GetPosition());
    }
    root->value = constraints.value;
    if (constraints.value < root->start) root->start = constraints.value;
    if (constraints.value > root->end) root->end = constraints.value;
  }
}

// Assigns values for the type itself, not including any children. Returns the
// next available value.
int SelectOwnValues(InstanceTypeTree* root, int start_value) {
  if (root->value == -1) {
    root->value = start_value;
  } else if (root->value < start_value) {
    Error("Failed to assign instance type ", root->value, " to ",
          root->type->name())
        .Position(root->type->GetPosition());
  }
  return root->value + root->num_own_values;
}

// Sorting function for types that don't have specific values they must include.
// Prioritizes bigger type ranges (those with more subtypes) first, and
// then sorts alphabetically within each size category.
struct CompareUnconstrainedTypes {
  constexpr bool operator()(const InstanceTypeTree* a,
                            const InstanceTypeTree* b) const {
    return (a->num_values > b->num_values)
               ? true
               : (a->num_values < b->num_values)
                     ? false
                     : std::less<std::string>()(a->type->name(),
                                                b->type->name());
  }
};

// Assigns concrete values for every instance type range, and sorts the children
// at each layer of the tree into increasing order. Appends the newly-assigned
// tree to the destination vector. Returns the first unassigned value after
// those that have been used.
int SolveInstanceTypeConstraints(
    std::unique_ptr<InstanceTypeTree> root, int start_value,
    std::vector<std::unique_ptr<InstanceTypeTree>>* destination) {
  if (root->start < start_value) {
    Error("Failed to assign instance type ", root->start, " to ",
          root->type->name())
        .Position(root->type->GetPosition());
  }

  // First, separate the children into four groups:
  // - The one child that must go first, if it exists;
  // - Children with specific value requirements ("constrained");
  // - Children without specific value requirements ("unconstrained");
  // - The one child that must go last, if it exists.
  std::unique_ptr<InstanceTypeTree> lowest_child;
  std::unique_ptr<InstanceTypeTree> highest_child;
  std::multimap<int, std::unique_ptr<InstanceTypeTree>>
      constrained_children_by_start;
  // Using std::map because you can't std::move out of a std::set until C++17.
  std::map<InstanceTypeTree*, std::unique_ptr<InstanceTypeTree>,
           CompareUnconstrainedTypes>
      unconstrained_children_by_size;
  for (auto& child : root->children) {
    if (child->type->IsHighestInstanceTypeWithinParent()) {
      if (highest_child) {
        Error("Two classes requested to be the highest instance type: ",
              highest_child->type->name(), " and ", child->type->name(),
              " within range for parent class ", root->type->name())
            .Position(child->type->GetPosition());
      }
      if (child->type->IsLowestInstanceTypeWithinParent()) {
        Error(
            "Class requested to be both highest and lowest instance type "
            "within its parent range: ",
            child->type->name())
            .Position(child->type->GetPosition());
      }
      highest_child = std::move(child);
    } else if (child->type->IsLowestInstanceTypeWithinParent()) {
      if (lowest_child) {
        Error("Two classes requested to be the lowest instance type: ",
              lowest_child->type->name(), " and ", child->type->name(),
              " within range for parent class ", root->type->name())
            .Position(child->type->GetPosition());
      }
      lowest_child = std::move(child);
    } else if (child->start > child->end) {
      unconstrained_children_by_size.insert(
          std::make_pair(child.get(), std::move(child)));
    } else {
      constrained_children_by_start.insert(
          std::make_pair(child->start, std::move(child)));
    }
  }
  root->children.clear();

  bool own_type_pending = root->num_own_values > 0;

  // Second, iterate and place the children in ascending order.
  if (lowest_child != nullptr) {
    start_value = SolveInstanceTypeConstraints(std::move(lowest_child),
                                               start_value, &root->children);
  }
  for (auto& constrained_child_pair : constrained_children_by_start) {
    // Select the next constrained child type in ascending order.
    std::unique_ptr<InstanceTypeTree> constrained_child =
        std::move(constrained_child_pair.second);

    // Try to place the root type before the constrained child type if it fits.
    if (own_type_pending) {
      if ((root->value != -1 && root->value < constrained_child->start) ||
          (root->value == -1 &&
           start_value + root->num_own_values <= constrained_child->start)) {
        start_value = SelectOwnValues(root.get(), start_value);
        own_type_pending = false;
      }
    }

    // Try to find any unconstrained children that fit before the constrained
    // one. This simple greedy algorithm just puts the biggest unconstrained
    // children in first, which might not fill the space as efficiently as
    // possible but is good enough for our needs.
    for (auto it = unconstrained_children_by_size.begin();
         it != unconstrained_children_by_size.end();) {
      if (it->second->num_values + start_value <= constrained_child->start) {
        start_value = SolveInstanceTypeConstraints(
            std::move(it->second), start_value, &root->children);
        it = unconstrained_children_by_size.erase(it);
      } else {
        ++it;
      }
    }

    // Place the constrained child type.
    start_value = SolveInstanceTypeConstraints(std::move(constrained_child),
                                               start_value, &root->children);
  }
  if (own_type_pending) {
    start_value = SelectOwnValues(root.get(), start_value);
    own_type_pending = false;
  }
  for (auto& child_pair : unconstrained_children_by_size) {
    start_value = SolveInstanceTypeConstraints(std::move(child_pair.second),
                                               start_value, &root->children);
  }
  if (highest_child != nullptr) {
    start_value = SolveInstanceTypeConstraints(std::move(highest_child),
                                               start_value, &root->children);
  }

  // Finally, set the range for this class to include all placed subclasses.
  root->end = start_value - 1;
  root->start =
      root->children.empty() ? start_value : root->children.front()->start;
  if (root->value != -1 && root->value < root->start) {
    root->start = root->value;
  }
  root->num_values = root->end - root->start + 1;
  root->type->InitializeInstanceTypes(
      root->value == -1 ? std::optional<int>{} : root->value,
      std::make_pair(root->start, root->end));

  if (root->num_values > 0) {
    destination->push_back(std::move(root));
  }
  return start_value;
}

std::unique_ptr<InstanceTypeTree> SolveInstanceTypeConstraints(
    std::unique_ptr<InstanceTypeTree> root) {
  std::vector<std::unique_ptr<InstanceTypeTree>> destination;
  SolveInstanceTypeConstraints(std::move(root), 0, &destination);
  return destination.empty() ? nullptr : std::move(destination.front());
}

std::unique_ptr<InstanceTypeTree> AssignInstanceTypes() {
  std::unique_ptr<InstanceTypeTree> root = BuildInstanceTypeTree();
  if (root != nullptr) {
    PropagateInstanceTypeConstraints(root.get());
    root = SolveInstanceTypeConstraints(std::move(root));
  }
  return root;
}

// Prints items in macro lists for the given type and its descendants.
// - definitions: pairs of instance type name and assigned value, such as
//   V(ODDBALL_TYPE, 67). Includes FIRST_*/LAST_* items for each type that has
//   more than one associated InstanceType, with items within those ranges
//   indented for readability.
// - values: just instance type names, like V(ODDBALL_TYPE). Does not include
//   any FIRST_*/LAST_* range markers.
// - debug_reader_classes_single: pairs of (ClassName, INSTANCE_TYPE) for
//   classes that have a Torque body (`extern class X extends Y { ... }`) and
//   a single corresponding instance type. Drives consumers that need to map
//   IT -> TqClass debug reader (debug_helper, gen-postmortem-metadata).
// - debug_reader_classes_multiple: same, for classes with a body and
//   subclasses sharing the IT.
// - debug_reader_classes_range: triples of (ClassName, FIRST_TYPE, LAST_TYPE)
//   for classes with a body spanning a contiguous IT range.
// Body-less Torque declarations (`extern class X extends Y;`) get no entry
// in the debug-reader lists -- Torque emits no TqClass for them.
// The `it_list_*` streams below reproduce metagen's
// INSTANCE_TYPE_LIST_{SINGLE,MULTIPLE,RANGE} buckets (instance_types.py)
// so the torque path (V8_USE_METAGEN_INSTANCE_TYPES=0) can drive the same
// instance-type-checker.h / instance-type-inl.h consumers as metagen.
// Unlike the debug-reader lists, these include body-less declarations
// (HasUndefinedLayout()), because the checkers map IT -> class for every
// IT-bearing class, not just those Torque has a layout for.
// TODO(jgruber): remove together with the V8_USE_METAGEN_INSTANCE_TYPES switch
// once metagen is the sole instance-type generator.
void PrintInstanceTypes(InstanceTypeTree* root, std::ostream& definitions,
                        std::ostream& values,
                        std::ostream& debug_reader_classes_single,
                        std::ostream& debug_reader_classes_multiple,
                        std::ostream& debug_reader_classes_range,
                        std::ostream& it_list_single,
                        std::ostream& it_list_multiple,
                        std::ostream& it_list_range,
                        const std::string& indent) {
  std::string type_name =
      CapifyStringWithUnderscores(root->type->name()) + "_TYPE";
  std::string inner_indent = indent;

  // Every type gets FIRST_/LAST_ range markers, aliasing the sole value for
  // single-instance-type classes. Generated CSA references the range markers
  // unconditionally (see GetClassInstanceTypeRange), so the single-vs-range
  // decision in DownCastForTorqueClass is made against the values in the
  // active instance-types.h (which may be metagen's, whose class hierarchy
  // can diverge from Torque's) instead of being baked in at Torque codegen
  // time.
  definitions << indent << "V(FIRST_" << type_name << ", " << root->start
              << ") \\\n";
  if (root->num_values > 1) {
    inner_indent += "  ";
  }
  if (root->num_own_values == 1) {
    definitions << inner_indent << "V(" << type_name << ", " << root->value
                << ") /* " << root->type->GetPosition() << " */\\\n";
    values << "  V(" << type_name << ") /* " << root->type->GetPosition()
           << " */\\\n";
    if (!root->type->DoNotGenerateInstanceTypeCheck()) {
      if (!root->type->HasUndefinedLayout()) {
        std::ostream& sink = root->num_values == 1
                                 ? debug_reader_classes_single
                                 : debug_reader_classes_multiple;
        sink << "  V(" << root->type->name() << ", " << type_name << ") /* "
             << root->type->GetPosition() << " */ \\\n";
      }
      std::ostream& it_sink =
          root->num_values == 1 ? it_list_single : it_list_multiple;
      it_sink << "  V(" << root->type->name() << ", " << type_name << ") /* "
              << root->type->GetPosition() << " */ \\\n";
    }
  }
  for (auto& child : root->children) {
    PrintInstanceTypes(
        child.get(), definitions, values, debug_reader_classes_single,
        debug_reader_classes_multiple, debug_reader_classes_range,
        it_list_single, it_list_multiple, it_list_range, inner_indent);
  }
  // We can't emit LAST_STRING_TYPE because it's not a valid flags
  // combination. So if the class type has multiple own values, which only
  // happens when using ANNOTATION_RESERVE_BITS_IN_INSTANCE_TYPE, then omit
  // the end marker.
  if (root->num_own_values <= 1) {
    definitions << indent << "V(LAST_" << type_name << ", " << root->end
                << ") \\\n";
  }

  if (root->num_values > 1) {
    // Only output the instance type range for things other than the root type.
    if (root->type->GetSuperClass() != nullptr &&
        !root->type->DoNotGenerateInstanceTypeCheck()) {
      if (!root->type->HasUndefinedLayout()) {
        debug_reader_classes_range << "  V(" << root->type->name() << ", FIRST_"
                                   << type_name << ", LAST_" << type_name
                                   << ") \\\n";
      }
      it_list_range << "  V(" << root->type->name() << ", FIRST_" << type_name
                    << ", LAST_" << type_name << ") \\\n";
    }
  }
}

}  // namespace

void ImplementationVisitor::GenerateInstanceTypes(
    const std::string& output_directory) {
  std::unique_ptr<InstanceTypeTree> instance_types = AssignInstanceTypes();
  std::stringstream assigned_instance_types;
  std::stringstream values_list;
  std::stringstream debug_reader_classes_single;
  std::stringstream debug_reader_classes_multiple;
  std::stringstream debug_reader_classes_range;
  std::stringstream it_list_single;
  std::stringstream it_list_multiple;
  std::stringstream it_list_range;
  if (instance_types != nullptr) {
    PrintInstanceTypes(instance_types.get(), assigned_instance_types,
                       values_list, debug_reader_classes_single,
                       debug_reader_classes_multiple,
                       debug_reader_classes_range, it_list_single,
                       it_list_multiple, it_list_range, "  ");
  }

  // Emit `torque-generated/instance-types.h`: IT enum + IT-name list. Pure
  // IT concern; consumed via src/objects/instance-types-gen.h's forwarder
  // when V8_USE_METAGEN_INSTANCE_TYPES is 0.
  std::stringstream header;
  const std::string file_name = "instance-types.h";
  {
    IncludeGuardScope guard(header, file_name);

    header << "// Instance types for all classes except for those that use "
              "InstanceType as flags.\n";
    header << "#define TORQUE_ASSIGNED_INSTANCE_TYPES(V) \\\n";
    header << assigned_instance_types.str();
    header << "\n";

    header << "// Instance types for all classes except for those that use\n";
    header << "// InstanceType as flags.\n";
    header << "#define TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(V) \\\n";
    header << values_list.str();
    header << "\n";
  }
  WriteFile(output_directory + "/" + file_name, header.str());

  // Emit `torque-generated/debug-reader-classes-list.h`: layout-availability
  // concern (which classes Torque has a body for, and therefore emits a
  // TqClass debug reader for). Consumed by tools/debug_helper and
  // tools/gen-postmortem-metadata.py. Separate file (and separate include
  // guard) from instance-types.h so consumers can include it independently
  // of the IT enum, which they get from metagen via the forwarder.
  std::stringstream debug_classes_header;
  const std::string debug_classes_file = "debug-reader-classes-list.h";
  {
    IncludeGuardScope debug_classes_guard(debug_classes_header,
                                          debug_classes_file);
    debug_classes_header
        << "// Pairs of (ClassName, INSTANCE_TYPE) for classes that have a\n"
           "// Torque body and a single corresponding instance type. The\n"
           "// INSTANCE_TYPE symbols resolve via the IT enum provided by\n"
           "// instance-types-gen.h (which forwards to metagen's emission\n"
           "// by default).\n";
    debug_classes_header
        << "#define TORQUE_DEBUG_READER_CLASSES_SINGLE(V) \\\n";
    debug_classes_header << debug_reader_classes_single.str();
    debug_classes_header << "\n";

    debug_classes_header
        << "// Same, for classes whose IT is also held by subclasses.\n";
    debug_classes_header
        << "#define TORQUE_DEBUG_READER_CLASSES_MULTIPLE(V) \\\n";
    debug_classes_header << debug_reader_classes_multiple.str();
    debug_classes_header << "\n";

    debug_classes_header
        << "// Triples of (ClassName, FIRST_TYPE, LAST_TYPE) for classes with\n"
           "// a Torque body that span a contiguous IT range.\n";
    debug_classes_header << "#define TORQUE_DEBUG_READER_CLASSES_RANGE(V) \\\n";
    debug_classes_header << debug_reader_classes_range.str();
    debug_classes_header << "\n";
  }
  WriteFile(output_directory + "/" + debug_classes_file,
            debug_classes_header.str());

  // Emit `torque-generated/instance-type-checker-lists.h`: the
  // INSTANCE_TYPE_LIST_{SINGLE,MULTIPLE,RANGE} buckets that metagen emits
  // into metagen/instance-types.h. Provided here so the torque path
  // (V8_USE_METAGEN_INSTANCE_TYPES=0) can satisfy the same consumers. It is
  // included only from src/objects/instance-types-gen.h's #else (torque)
  // branch and is additionally guarded below, so the metagen path never
  // sees it. Remove this file (and the #else include) once metagen is the
  // sole instance-type generator.
  std::stringstream it_lists_header;
  const std::string it_lists_file = "instance-type-checker-lists.h";
  {
    IncludeGuardScope it_lists_guard(it_lists_header, it_lists_file);
    it_lists_header << "#if !V8_USE_METAGEN_INSTANCE_TYPES\n";
    it_lists_header
        << "// Pairs of (ClassName, INSTANCE_TYPE) for classes whose instance\n"
           "// type is unique to them (no subclasses share it).\n";
    it_lists_header << "#define INSTANCE_TYPE_LIST_SINGLE(V) \\\n";
    it_lists_header << it_list_single.str();
    it_lists_header << "\n";

    it_lists_header << "// Pairs of (ClassName, INSTANCE_TYPE) for classes "
                       "that have their\n"
                       "// own instance type and whose subclasses share it.\n";
    it_lists_header << "#define INSTANCE_TYPE_LIST_MULTIPLE(V) \\\n";
    it_lists_header << it_list_multiple.str();
    it_lists_header << "\n";

    it_lists_header
        << "// Triples of (ClassName, FIRST_TYPE, LAST_TYPE) for classes that\n"
           "// span a contiguous range of instance types.\n";
    it_lists_header << "#define INSTANCE_TYPE_LIST_RANGE(V) \\\n";
    it_lists_header << it_list_range.str();
    it_lists_header << "\n";
    it_lists_header << "#endif  // !V8_USE_METAGEN_INSTANCE_TYPES\n";
  }
  WriteFile(output_directory + "/" + it_lists_file, it_lists_header.str());

  GlobalContext::SetInstanceTypesInitialized();
}

}  // namespace v8::internal::torque
