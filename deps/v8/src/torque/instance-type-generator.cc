// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/implementation-visitor.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

// Contains all necessary state for a single class type during the process of
// assigning instance types, and provides a convenient way to access the list of
// types that inherit from this one.
struct InstanceTypeTree {
  explicit InstanceTypeTree(const ClassType* type)
      : type(type),
        parent(nullptr),
        start(INT_MAX),
        end(INT_MIN),
        value(-1),
        num_values(0),
        num_own_values(0) {}
  const ClassType* type;
  InstanceTypeTree* parent;
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
      if (root != nullptr)
        Error("Expected only one root class type. Found: ", root->type->name(),
              " and ", type_tree->type->name())
            .Position(type_tree->type->GetPosition());
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
  if ((!root->type->IsAbstract() ||
       root->type->IsInstantiatedAbstractClass()) &&
      !root->type->HasSameInstanceTypeAsParent()) {
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
// - definitions: This list is pairs of instance type name and assigned value,
//   such as V(ODDBALL_TYPE, 67). It includes FIRST_* and LAST_* items for each
//   type that has more than one associated InstanceType. Items within those
//   ranges are indented for readability.
// - values: This list is just instance type names, like V(ODDBALL_TYPE). It
//   does not include any FIRST_* and LAST_* range markers.
void PrintInstanceTypes(InstanceTypeTree* root, std::ostream& definitions,
                        std::ostream& values, const std::string& indent) {
  std::string type_name =
      CapifyStringWithUnderscores(root->type->name()) + "_TYPE";
  std::string inner_indent = indent;

  if (root->num_values > 1) {
    definitions << indent << "V(FIRST_" << type_name << ", " << root->start
                << ") \\\n";
    inner_indent += "  ";
  }
  if (root->num_own_values == 1) {
    definitions << inner_indent << "V(" << type_name << ", " << root->value
                << ") \\\n";
    values << "  V(" << type_name << ") \\\n";
  }
  for (auto& child : root->children) {
    PrintInstanceTypes(child.get(), definitions, values, inner_indent);
  }
  // We can't emit LAST_STRING_TYPE because it's not a valid flags combination.
  // So if the class type has multiple own values, which only happens when using
  // ANNOTATION_RESERVE_BITS_IN_INSTANCE_TYPE, then omit the end marker.
  if (root->num_values > 1 && root->num_own_values <= 1) {
    definitions << indent << "V(LAST_" << type_name << ", " << root->end
                << ") \\\n";
  }
}

}  // namespace

void ImplementationVisitor::GenerateInstanceTypes(
    const std::string& output_directory) {
  std::stringstream header;
  std::string file_name = "instance-types-tq.h";
  {
    IncludeGuardScope guard(header, file_name);

    header << "// Instance types for all classes except for those that use "
              "InstanceType as flags.\n";
    header << "#define TORQUE_ASSIGNED_INSTANCE_TYPES(V) \\\n";
    std::unique_ptr<InstanceTypeTree> instance_types = AssignInstanceTypes();
    std::stringstream values_list;
    if (instance_types != nullptr) {
      PrintInstanceTypes(instance_types.get(), header, values_list, "  ");
    }
    header << "\n\n";

    header << "// Instance types for all classes except for those that use "
              "InstanceType as flags.\n";
    header << "#define TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(V) \\\n";
    header << values_list.str();
    header << "\n\n";

    header << "// Instance types for Torque-internal classes.\n";
    header << "#define TORQUE_INTERNAL_INSTANCE_TYPES(V) \\\n";
    for (const TypeAlias* alias : GlobalContext::GetClasses()) {
      const ClassType* type = ClassType::DynamicCast(alias->type());
      if (type->IsExtern()) continue;
      std::string type_name =
          CapifyStringWithUnderscores(type->name()) + "_TYPE";
      header << "  V(" << type_name << ") \\\n";
    }
    header << "\n\n";

    header << "// Struct list entries for Torque-internal classes.\n";
    header << "#define TORQUE_STRUCT_LIST_GENERATOR(V, _) \\\n";
    for (const TypeAlias* alias : GlobalContext::GetClasses()) {
      const ClassType* type = ClassType::DynamicCast(alias->type());
      if (type->IsExtern()) continue;
      std::string type_name =
          CapifyStringWithUnderscores(type->name()) + "_TYPE";
      std::string variable_name = SnakeifyString(type->name());
      header << "  V(_, " << type_name << ", " << type->name() << ", "
             << variable_name << ") \\\n";
    }
    header << "\n";
  }
  std::string output_header_path = output_directory + "/" + file_name;
  WriteFile(output_header_path, header.str());
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
