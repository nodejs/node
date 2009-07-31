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


// Initlialize namespaces
var devtools = devtools || {};
devtools.profiler = devtools.profiler || {};


/**
 * Constructs a mapper that maps addresses into code entries.
 *
 * @constructor
 */
devtools.profiler.CodeMap = function() {
  /**
   * Dynamic code entries. Used for JIT compiled code.
   */
  this.dynamics_ = new goog.structs.SplayTree();

  /**
   * Name generator for entries having duplicate names.
   */
  this.dynamicsNameGen_ = new devtools.profiler.CodeMap.NameGenerator();

  /**
   * Static code entries. Used for statically compiled code.
   */
  this.statics_ = new goog.structs.SplayTree();

  /**
   * Libraries entries. Used for the whole static code libraries.
   */
  this.libraries_ = new goog.structs.SplayTree();

  /**
   * Map of memory pages occupied with static code.
   */
  this.pages_ = [];
};


/**
 * The number of alignment bits in a page address.
 */
devtools.profiler.CodeMap.PAGE_ALIGNMENT = 12;


/**
 * Page size in bytes.
 */
devtools.profiler.CodeMap.PAGE_SIZE =
    1 << devtools.profiler.CodeMap.PAGE_ALIGNMENT;


/**
 * Adds a dynamic (i.e. moveable and discardable) code entry.
 *
 * @param {number} start The starting address.
 * @param {devtools.profiler.CodeMap.CodeEntry} codeEntry Code entry object.
 */
devtools.profiler.CodeMap.prototype.addCode = function(start, codeEntry) {
  this.dynamics_.insert(start, codeEntry);
};


/**
 * Moves a dynamic code entry. Throws an exception if there is no dynamic
 * code entry with the specified starting address.
 *
 * @param {number} from The starting address of the entry being moved.
 * @param {number} to The destination address.
 */
devtools.profiler.CodeMap.prototype.moveCode = function(from, to) {
  var removedNode = this.dynamics_.remove(from);
  this.dynamics_.insert(to, removedNode.value);
};


/**
 * Discards a dynamic code entry. Throws an exception if there is no dynamic
 * code entry with the specified starting address.
 *
 * @param {number} start The starting address of the entry being deleted.
 */
devtools.profiler.CodeMap.prototype.deleteCode = function(start) {
  var removedNode = this.dynamics_.remove(start);
};


/**
 * Adds a library entry.
 *
 * @param {number} start The starting address.
 * @param {devtools.profiler.CodeMap.CodeEntry} codeEntry Code entry object.
 */
devtools.profiler.CodeMap.prototype.addLibrary = function(
    start, codeEntry) {
  this.markPages_(start, start + codeEntry.size);
  this.libraries_.insert(start, codeEntry);
};


/**
 * Adds a static code entry.
 *
 * @param {number} start The starting address.
 * @param {devtools.profiler.CodeMap.CodeEntry} codeEntry Code entry object.
 */
devtools.profiler.CodeMap.prototype.addStaticCode = function(
    start, codeEntry) {
  this.statics_.insert(start, codeEntry);
};


/**
 * @private
 */
devtools.profiler.CodeMap.prototype.markPages_ = function(start, end) {
  for (var addr = start; addr <= end;
       addr += devtools.profiler.CodeMap.PAGE_SIZE) {
    this.pages_[addr >>> devtools.profiler.CodeMap.PAGE_ALIGNMENT] = 1;
  }
};


/**
 * @private
 */
devtools.profiler.CodeMap.prototype.isAddressBelongsTo_ = function(addr, node) {
  return addr >= node.key && addr < (node.key + node.value.size);
};


/**
 * @private
 */
devtools.profiler.CodeMap.prototype.findInTree_ = function(tree, addr) {
  var node = tree.findGreatestLessThan(addr);
  return node && this.isAddressBelongsTo_(addr, node) ? node.value : null;
};


/**
 * Finds a code entry that contains the specified address. Both static and
 * dynamic code entries are considered.
 *
 * @param {number} addr Address.
 */
devtools.profiler.CodeMap.prototype.findEntry = function(addr) {
  var pageAddr = addr >>> devtools.profiler.CodeMap.PAGE_ALIGNMENT;
  if (pageAddr in this.pages_) {
    // Static code entries can contain "holes" of unnamed code.
    // In this case, the whole library is assigned to this address.
    return this.findInTree_(this.statics_, addr) ||
        this.findInTree_(this.libraries_, addr);
  }
  var min = this.dynamics_.findMin();
  var max = this.dynamics_.findMax();
  if (max != null && addr < (max.key + max.value.size) && addr >= min.key) {
    var dynaEntry = this.findInTree_(this.dynamics_, addr);
    if (dynaEntry == null) return null;
    // Dedupe entry name.
    if (!dynaEntry.nameUpdated_) {
      dynaEntry.name = this.dynamicsNameGen_.getName(dynaEntry.name);
      dynaEntry.nameUpdated_ = true;
    }
    return dynaEntry;
  }
  return null;
};


/**
 * Returns an array of all dynamic code entries.
 */
devtools.profiler.CodeMap.prototype.getAllDynamicEntries = function() {
  return this.dynamics_.exportValues();
};


/**
 * Returns an array of all static code entries.
 */
devtools.profiler.CodeMap.prototype.getAllStaticEntries = function() {
  return this.statics_.exportValues();
};


/**
 * Returns an array of all libraries entries.
 */
devtools.profiler.CodeMap.prototype.getAllLibrariesEntries = function() {
  return this.libraries_.exportValues();
};


/**
 * Creates a code entry object.
 *
 * @param {number} size Code entry size in bytes.
 * @param {string} opt_name Code entry name.
 * @constructor
 */
devtools.profiler.CodeMap.CodeEntry = function(size, opt_name) {
  this.size = size;
  this.name = opt_name || '';
  this.nameUpdated_ = false;
};


devtools.profiler.CodeMap.CodeEntry.prototype.getName = function() {
  return this.name;
};


devtools.profiler.CodeMap.CodeEntry.prototype.toString = function() {
  return this.name + ': ' + this.size.toString(16);
};


devtools.profiler.CodeMap.NameGenerator = function() {
  this.knownNames_ = [];
};


devtools.profiler.CodeMap.NameGenerator.prototype.getName = function(name) {
  if (!(name in this.knownNames_)) {
    this.knownNames_[name] = 0;
    return name;
  }
  var count = ++this.knownNames_[name];
  return name + ' {' + count + '}';
};
