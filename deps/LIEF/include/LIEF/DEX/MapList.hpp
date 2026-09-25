/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_MAP_LIST_H
#define LIEF_MAP_LIST_H
#include <map>
#include <vector>

#include "LIEF/visibility.h"
#include "LIEF/iterators.hpp"
#include "LIEF/Object.hpp"

#include "LIEF/DEX/MapItem.hpp"

namespace LIEF {
namespace DEX {
class Parser;
class Class;

/// Class which represents the ``map_list`` structure that
/// follows the main DEX header.
///
/// This MapList aims at referencing the location of other DEX structures as
/// described in https://source.android.com/devices/tech/dalvik/dex-format#map-item
class LIEF_API MapList : public Object {
  friend class Parser;

  public:
  using items_t           = std::map<MapItem::TYPES, MapItem>;
  using it_items_t        = ref_iterator<std::vector<MapItem*>>;
  using it_const_items_t  = const_ref_iterator<std::vector<MapItem*>>;

  public:
  MapList();

  MapList(const MapList&);
  MapList& operator=(const MapList&);

  /// Iterator over LIEF::DEX::MapItem
  it_items_t items();
  it_const_items_t items() const;

  /// Check if the given type exists
  bool has(MapItem::TYPES type) const;

  /// Return the LIEF::DEX::MapItem associated with the given type
  const MapItem& get(MapItem::TYPES type) const;

  /// Return the LIEF::DEX::MapItem associated with the given type
  MapItem& get(MapItem::TYPES type);

  /// Return the LIEF::DEX::MapItem associated with the given type
  const MapItem& operator[](MapItem::TYPES type) const;

  /// Return the LIEF::DEX::MapItem associated with the given type
  MapItem& operator[](MapItem::TYPES type);

  void accept(Visitor& visitor) const override;


  LIEF_API friend std::ostream& operator<<(std::ostream& os, const MapList& mtd);

  ~MapList() override;

  private:
  items_t items_;

};

} // Namespace DEX
} // Namespace LIEF
#endif
