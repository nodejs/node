// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import { SplayTree } from "./splaytree.mjs";

/**
 * Constructs a mapper that maps addresses into code entries.
 *
 * @constructor
 */
export function CodeMap() {
  /**
   * Dynamic code entries. Used for JIT compiled code.
   */
  this.dynamics_ = new SplayTree();

  /**
   * Name generator for entries having duplicate names.
   */
  this.dynamicsNameGen_ = new CodeMap.NameGenerator();

  /**
   * Static code entries. Used for statically compiled code.
   */
  this.statics_ = new SplayTree();

  /**
   * Libraries entries. Used for the whole static code libraries.
   */
  this.libraries_ = new SplayTree();

  /**
   * Map of memory pages occupied with static code.
   */
  this.pages_ = [];
};


/**
 * The number of alignment bits in a page address.
 */
CodeMap.PAGE_ALIGNMENT = 12;


/**
 * Page size in bytes.
 */
CodeMap.PAGE_SIZE =
    1 << CodeMap.PAGE_ALIGNMENT;


/**
 * Adds a dynamic (i.e. moveable and discardable) code entry.
 *
 * @param {number} start The starting address.
 * @param {CodeMap.CodeEntry} codeEntry Code entry object.
 */
CodeMap.prototype.addCode = function(start, codeEntry) {
  this.deleteAllCoveredNodes_(this.dynamics_, start, start + codeEntry.size);
  this.dynamics_.insert(start, codeEntry);
};


/**
 * Moves a dynamic code entry. Throws an exception if there is no dynamic
 * code entry with the specified starting address.
 *
 * @param {number} from The starting address of the entry being moved.
 * @param {number} to The destination address.
 */
CodeMap.prototype.moveCode = function(from, to) {
  var removedNode = this.dynamics_.remove(from);
  this.deleteAllCoveredNodes_(this.dynamics_, to, to + removedNode.value.size);
  this.dynamics_.insert(to, removedNode.value);
};


/**
 * Discards a dynamic code entry. Throws an exception if there is no dynamic
 * code entry with the specified starting address.
 *
 * @param {number} start The starting address of the entry being deleted.
 */
CodeMap.prototype.deleteCode = function(start) {
  var removedNode = this.dynamics_.remove(start);
};


/**
 * Adds a library entry.
 *
 * @param {number} start The starting address.
 * @param {CodeMap.CodeEntry} codeEntry Code entry object.
 */
CodeMap.prototype.addLibrary = function(
    start, codeEntry) {
  this.markPages_(start, start + codeEntry.size);
  this.libraries_.insert(start, codeEntry);
};


/**
 * Adds a static code entry.
 *
 * @param {number} start The starting address.
 * @param {CodeMap.CodeEntry} codeEntry Code entry object.
 */
CodeMap.prototype.addStaticCode = function(
    start, codeEntry) {
  this.statics_.insert(start, codeEntry);
};


/**
 * @private
 */
CodeMap.prototype.markPages_ = function(start, end) {
  for (var addr = start; addr <= end;
       addr += CodeMap.PAGE_SIZE) {
    this.pages_[(addr / CodeMap.PAGE_SIZE)|0] = 1;
  }
};


/**
 * @private
 */
CodeMap.prototype.deleteAllCoveredNodes_ = function(tree, start, end) {
  var to_delete = [];
  var addr = end - 1;
  while (addr >= start) {
    var node = tree.findGreatestLessThan(addr);
    if (!node) break;
    var start2 = node.key, end2 = start2 + node.value.size;
    if (start2 < end && start < end2) to_delete.push(start2);
    addr = start2 - 1;
  }
  for (var i = 0, l = to_delete.length; i < l; ++i) tree.remove(to_delete[i]);
};


/**
 * @private
 */
CodeMap.prototype.isAddressBelongsTo_ = function(addr, node) {
  return addr >= node.key && addr < (node.key + node.value.size);
};


/**
 * @private
 */
CodeMap.prototype.findInTree_ = function(tree, addr) {
  var node = tree.findGreatestLessThan(addr);
  return node && this.isAddressBelongsTo_(addr, node) ? node : null;
};


/**
 * Finds a code entry that contains the specified address. Both static and
 * dynamic code entries are considered. Returns the code entry and the offset
 * within the entry.
 *
 * @param {number} addr Address.
 */
CodeMap.prototype.findAddress = function(addr) {
  var pageAddr = (addr / CodeMap.PAGE_SIZE)|0;
  if (pageAddr in this.pages_) {
    // Static code entries can contain "holes" of unnamed code.
    // In this case, the whole library is assigned to this address.
    var result = this.findInTree_(this.statics_, addr);
    if (!result) {
      result = this.findInTree_(this.libraries_, addr);
      if (!result) return null;
    }
    return { entry : result.value, offset : addr - result.key };
  }
  var min = this.dynamics_.findMin();
  var max = this.dynamics_.findMax();
  if (max != null && addr < (max.key + max.value.size) && addr >= min.key) {
    var dynaEntry = this.findInTree_(this.dynamics_, addr);
    if (dynaEntry == null) return null;
    // Dedupe entry name.
    var entry = dynaEntry.value;
    if (!entry.nameUpdated_) {
      entry.name = this.dynamicsNameGen_.getName(entry.name);
      entry.nameUpdated_ = true;
    }
    return { entry : entry, offset : addr - dynaEntry.key };
  }
  return null;
};


/**
 * Finds a code entry that contains the specified address. Both static and
 * dynamic code entries are considered.
 *
 * @param {number} addr Address.
 */
CodeMap.prototype.findEntry = function(addr) {
  var result = this.findAddress(addr);
  return result ? result.entry : null;
};


/**
 * Returns a dynamic code entry using its starting address.
 *
 * @param {number} addr Address.
 */
CodeMap.prototype.findDynamicEntryByStartAddress =
    function(addr) {
  var node = this.dynamics_.find(addr);
  return node ? node.value : null;
};


/**
 * Returns an array of all dynamic code entries.
 */
CodeMap.prototype.getAllDynamicEntries = function() {
  return this.dynamics_.exportValues();
};


/**
 * Returns an array of pairs of all dynamic code entries and their addresses.
 */
CodeMap.prototype.getAllDynamicEntriesWithAddresses = function() {
  return this.dynamics_.exportKeysAndValues();
};


/**
 * Returns an array of all static code entries.
 */
CodeMap.prototype.getAllStaticEntries = function() {
  return this.statics_.exportValues();
};


/**
 * Returns an array of pairs of all static code entries and their addresses.
 */
CodeMap.prototype.getAllStaticEntriesWithAddresses = function() {
  return this.statics_.exportKeysAndValues();
};


/**
 * Returns an array of all libraries entries.
 */
CodeMap.prototype.getAllLibrariesEntries = function() {
  return this.libraries_.exportValues();
};


/**
 * Creates a code entry object.
 *
 * @param {number} size Code entry size in bytes.
 * @param {string} opt_name Code entry name.
 * @param {string} opt_type Code entry type, e.g. SHARED_LIB, CPP.
 * @constructor
 */
CodeMap.CodeEntry = function(size, opt_name, opt_type) {
  this.size = size;
  this.name = opt_name || '';
  this.type = opt_type || '';
  this.nameUpdated_ = false;
};


CodeMap.CodeEntry.prototype.getName = function() {
  return this.name;
};


CodeMap.CodeEntry.prototype.toString = function() {
  return this.name + ': ' + this.size.toString(16);
};


CodeMap.NameGenerator = function() {
  this.knownNames_ = {};
};


CodeMap.NameGenerator.prototype.getName = function(name) {
  if (!(name in this.knownNames_)) {
    this.knownNames_[name] = 0;
    return name;
  }
  var count = ++this.knownNames_[name];
  return name + ' {' + count + '}';
};
