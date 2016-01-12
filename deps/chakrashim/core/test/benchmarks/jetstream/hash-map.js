/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE below for additional
 *  information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/******* NOTICE *********

Apache Harmony
Copyright 2006, 2010 The Apache Software Foundation.

This product includes software developed at
The Apache Software Foundation (http://www.apache.org/).

Portions of Apache Harmony were originally developed by
Intel Corporation and are licensed to the Apache Software
Foundation under the "Software Grant and Corporate Contribution
License Agreement" and for which the following copyright notices
apply
         (C) Copyright 2005 Intel Corporation
         (C) Copyright 2005-2006 Intel Corporation
         (C) Copyright 2006 Intel Corporation


The following copyright notice(s) were affixed to portions of the code
with which this file is now or was at one time distributed
and are placed here unaltered.

(C) Copyright 1997,2004 International Business Machines Corporation.
All rights reserved.

(C) Copyright IBM Corp. 2003. 


This software contains code derived from UNIX V7, Copyright(C)
Caldera International Inc.

************************/

var referenceScore = 565;

if (typeof (WScript) === "undefined") {
    var WScript = {
        Echo: print
    }
}
var print = function () {};

var performance = performance || {};
performance.now = (function() {
  return performance.now       ||
         performance.mozNow    ||
         performance.msNow     ||
         performance.oNow      ||
         performance.webkitNow ||
         Date.now;
})();


function reportResult(score) {
    WScript.Echo("### SCORE: " + (100 * referenceScore / score));
}

var top = {};
top.JetStream = {
    goodTime: performance.now,
    reportResult: reportResult
};

var __time_before = top.JetStream.goodTime();

////////////////////////////////////////////////////////////////////////////////
// Test
////////////////////////////////////////////////////////////////////////////////

//@ runDefault

// This code is a manual translation of Apache Harmony's HashMap class to
// JavaScript.

var HashMap = (function() {
    var DEFAULT_SIZE = 16;
    
    function calculateCapacity(x)
    {
        if (x >= 1 << 30)
            return 1 << 30;
        if (x == 0)
            return 16;
        x = x - 1;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        return x + 1;
    }
    
    function computeHashCode(key)
    {
        switch (typeof key) {
        case "undefined":
            return 0;
        case "object":
            if (!key)
                return 0;
        case "function":
            return key.hashCode();
        case "boolean":
            return key | 0;
        case "number":
            if ((key | 0) == key)
                return key;
            key = "" + key; // Compute string hash of the double. It's the best we can do.
        case "string":
            // source: http://en.wikipedia.org/wiki/Java_hashCode()#Sample_implementations_of_the_java.lang.String_algorithm
            var h = 0;
            var len = key.length;
            for (var index = 0; index < len; index++) {
                h = (((31 * h) | 0) + key.charCodeAt(index)) | 0;
            }
            return h;
        default:
            throw new Error("Internal error: Bad JavaScript value type");
        }
    }
    
    function equals(a, b)
    {
        if (typeof a != typeof b)
            return false;
        switch (typeof a) {
        case "object":
            if (!a)
                return !b;
        case "function":
            switch (typeof b) {
            case "object":
            case "function":
                return a.equals(b);
            default:
                return false;
            }
        default:
            return a == b;
        }
    }
    
    function Entry(key, hash, value)
    {
        this._key = key;
        this._value = value;
        this._origKeyHash = hash;
        this._next = null;
    }

    Entry.prototype = {
        clone: function()
        {
            var result = new Entry(this._key, this._hash, this._value);
            if (this._next)
                result._next = this._next.clone();
            return result;
        },
        
        toString: function()
        {
            return this._key + "=" + this._value;
        },
        
        get key()
        {
            return this._key;
        },
        
        get value()
        {
            return this._value;
        }
    };
    
    function AbstractMapIterator(map)
    {
        this._associatedMap = map;
        this._expectedModCount = map._modCount;
        this._futureEntry = null;
        this._currentEntry = null;
        this._prevEntry = null;
        this._position = 0;
    }
    
    AbstractMapIterator.prototype = {
        hasNext: function()
        {
            if (this._futureEntry)
                return true;
            while (this._position < this._associatedMap._elementData.length) {
                if (!this._associatedMap._elementData[this._position])
                    this._position++;
                else
                    return true;
            }
            return false;
        },
        
        _checkConcurrentMod: function()
        {
            if (this._expectedModCount != this._associatedMap._modCount)
                throw new Error("Concurrent HashMap modification detected");
        },
        
        _makeNext: function()
        {
            this._checkConcurrentMod();
            if (!this.hasNext())
                throw new Error("No such element");
            if (!this._futureEntry) {
                this._currentEntry = this._associatedMap._elementData[this._position++];
                this._futureEntry = this._currentEntry._next;
                this._prevEntry = null;
                return;
            }
            if (this._currentEntry)
                this._prevEntry = this._currentEntry;
            this._currentEntry = this._futureEntry;
            this._futureEntry = this._futureEntry._next;
        },
        
        remove: function()
        {
            this._checkConcurrentMod();
            if (!this._currentEntry)
                throw new Error("Illegal state");
            if (!this._prevEntry) {
                var index = this._currentEntry._origKeyHash & (this._associatedMap._elementData.length - 1);
                this._associatedMap._elementData[index] = this._associatedMap._elementData[index]._next;
            } else
                this._prevEntry._next = this._currentEntry._next;
            this._currentEntry = null;
            this._expectedModCount++;
            this._associatedMap._modCount++;
            this._associatedMap._elementCount--;
        }
    };
    
    function EntryIterator(map)
    {
        AbstractMapIterator.call(this, map);
    }
    
    EntryIterator.prototype = {
        next: function()
        {
            this._makeNext();
            return this._currentEntry;
        }
    };
    EntryIterator.prototype.__proto__ = AbstractMapIterator.prototype;
    
    function KeyIterator(map)
    {
        AbstractMapIterator.call(this, map);
    }
    
    KeyIterator.prototype = {
        next: function()
        {
            this.makeNext();
            return this._currentEntry._key;
        }
    };
    KeyIterator.prototype.__proto__ = AbstractMapIterator.prototype;
    
    function ValueIterator(map)
    {
        AbstractMapIterator.call(this, map);
    }
    
    ValueIterator.prototype = {
        next: function()
        {
            this.makeNext();
            return this._currentEntry._value;
        }
    };
    ValueIterator.prototype.__proto__ = AbstractMapIterator.prototype;
    
    function EntrySet(map)
    {
        this._associatedMap = map;
    }
    
    EntrySet.prototype = {
        size: function()
        {
            return this._associatedMap._elementCount;
        },
        
        clear: function()
        {
            this._associatedMap.clear();
        },
        
        remove: function(object)
        {
            var entry = this._associatedMap._getEntry(object.key);
            if (!entry)
                return false;
            if (!equals(entry._value, object.value))
                return false;
            this._associatedMap._removeEntry(entry);
            return true;
        },
        
        contains: function(object)
        {
            var entry = this._associatedMap._getEntry(object.key);
            if (!entry)
                return false;
            return equals(entry._value, object.value);
        },
        
        iterator: function()
        {
            return new EntryIterator(this._associatedMap);
        }
    };
    
    function KeySet(map)
    {
        this._associatedMap = map;
    }
    
    KeySet.prototype = {
        contains: function(object)
        {
            return this._associatedMap.contains(object);
        },
        
        size: function()
        {
            return this._associatedMap._elementCount;
        },
        
        clear: function()
        {
            this._associatedMap.clear();
        },
        
        remove: function(key)
        {
            return !!this._associatedMap.remove(key);
        },
        
        iterator: function()
        {
            return new KeyIterator(this._associatedMap);
        }
    };
    
    function ValueCollection(map)
    {
        this._associatedMap = map;
    }
    
    ValueCollection.prototype = {
        contains: function(object)
        {
            return this._associatedMap.containsValue(object);
        },
        
        size: function()
        {
            return this._associatedMap._elementCount;
        },
        
        clear: function()
        {
            this._associatedMap.clear();
        },
        
        iterator: function()
        {
            return new ValueIterator(this._associatedMap);
        }
    };
    
    function HashMap(capacity, loadFactor)
    {
        if (capacity == null)
            capacity = DEFAULT_SIZE;
        if (loadFactor == null)
            loadFactor = 0.75;
        
        if (capacity < 0)
            throw new Error("Invalid argument to HashMap constructor: capacity is negative");
        if (loadFactor <= 0)
            throw new Error("Invalid argument to HashMap constructor: loadFactor is not positive");
        
        this._capacity = calculateCapacity(capacity);
        this._elementCount = 0;
        this._elementData = new Array(this.capacity);
        this._loadFactor = loadFactor;
        this._modCount = 0;
        this._computeThreshold();
    }
    
    HashMap.prototype = {
        _computeThreshold: function()
        {
            this._threshold = (this._elementData.length * this._loadFactor) | 0;
        },
        
        clear: function()
        {
            if (!this._elementCount)
                return;
            
            this._elementCount = 0;
            for (var i = this._elementData.length; i--;)
                this._elementData[i] = null;
            this._modCount++;
        },
        
        clone: function()
        {
            var result = new HashMap(this._elementData.length, this._loadFactor);
            result.putAll(this);
            return result;
        },
        
        containsKey: function(key)
        {
            return !!this._getEntry(key);
        },
        
        containsValue: function(value)
        {
            for (var i = this._elementData.length; i--;) {
                for (var entry = this._elementData[i]; entry; entry = entry._next) {
                    if (equals(value, entry._value))
                        return true;
                }
            }
            return false;
        },
        
        entrySet: function()
        {
            return new EntrySet(this);
        },
        
        get: function(key)
        {
            var entry = this._getEntry(key);
            if (entry)
                return entry._value;
            return null;
        },
        
        _getEntry: function(key)
        {
            var hash = computeHashCode(key);
            var index = hash & (this._elementData.length - 1);
            return this._findKeyEntry(key, index, hash);
        },
        
        _findKeyEntry: function(key, index, keyHash)
        {
            var entry = this._elementData[index];
            while (entry && (entry._origKeyHash != keyHash || !equals(key, entry._key)))
                entry = entry._next;
            return entry;
        },
        
        isEmpty: function()
        {
            return !this._elementCount;
        },
        
        keySet: function()
        {
            return new KeySet(this);
        },
        
        put: function(key, value)
        {
            var hash = computeHashCode(key);
            var index = hash & (this._elementData.length - 1);
            var entry = this._findKeyEntry(key, index, hash);
            if (!entry) {
                this._modCount++;
                entry = this._createHashedEntry(key, index, hash);
                if (++this._elementCount > this._threshold)
                    this._rehash();
            }
            
            var result = entry._value;
            entry._value = value;
            return result;
        },
        
        _createHashedEntry: function(key, index, hash)
        {
            var entry = new Entry(key, hash, null);
            entry._next = this._elementData[index];
            this._elementData[index] = entry;
            return entry;
        },
        
        putAll: function(map)
        {
            if (map.isEmpty())
                return;
            for (var iter = map.entrySet().iterator(); iter.hasNext();) {
                var entry = iter.next();
                put(entry.key, entry.value);
            }
        },
        
        _rehash: function(capacity)
        {
            if (capacity == null)
                capacity = this._elementData.length;
            
            var length = calculateCapacity(!capacity ? 1 : capacity << 1);
            var newData = new Array(length);
            for (var i = 0; i < this._elementData.length; ++i) {
                var entry = this._elementData[i];
                this._elementData[i] = null;
                while (entry) {
                    var index = entry._origKeyHash & (length - 1);
                    var next = entry._next;
                    entry._next = newData[index];
                    newData[index] = entry;
                    entry = next;
                }
            }
            this._elementData = newData;
            this._computeThreshold();
        },
        
        remove: function(key)
        {
            var entry = this._removeEntryForKey(key);
            if (!entry)
                return null;
            return entry._value;
        },
        
        _removeEntry: function(entry)
        {
            var index = entry._origKeyHash & (this._elementData.length - 1);
            var current = this._elementData[index];
            if (current == entry)
                this._elementData[index] = entry._next;
            else {
                while (current._next != entry)
                    current = current._next;
                current._next = entry._next;
            }
            this._modCount++;
            this._elementCount--;
        },
        
        _removeEntryForKey: function(key)
        {
            var hash = computeHashCode(key);
            var index = hash & (this._elementData.length - 1);
            var entry = this._elementData[index];
            var last = null;
            while (entry != null && !(entry._origKeyHash == hash && equals(key, entry._key))) {
                last = entry;
                entry = entry._next;
            }
            if (!entry)
                return null;
            if (!last)
                this._elementData[index] = entry._next;
            else
                last._next = entry._next;
            this._modCount++;
            this._elementCount--;
            return entry;
        },
        
        size: function()
        {
            return this._elementCount;
        },
        
        values: function()
        {
            return new ValueCollection(this);
        }
    };
    
    return HashMap;
})();

var map = new HashMap();
var COUNT = 500000;

for (var i = 0; i < COUNT; ++i)
    map.put(i, 42);

var result = 0;
for (var j = 0; j < 5; ++j) {
    for (var i = 0; i < COUNT; ++i)
        result += map.get(i);
}

var keySum = 0;
var valueSum = 0;
for (var iterator = map.entrySet().iterator(); iterator.hasNext();) {
    var entry = iterator.next();
    keySum += entry.key;
    valueSum += entry.value;
}

if (result != 105000000)
    throw "Error: result = " + result;
if (keySum != 124999750000)
    throw "Error: keySum = " + keySum;
if (valueSum != 21000000)
    throw "Error: valueSum = " + valueSum;

////////////////////////////////////////////////////////////////////////////////
// Runner
////////////////////////////////////////////////////////////////////////////////
var __time_after = top.JetStream.goodTime();
top.JetStream.reportResult(__time_after - __time_before);
