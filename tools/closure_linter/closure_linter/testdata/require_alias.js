// We are missing a require of goog.sample.UsedType
goog.provide('goog.something.Else'); // +1: MISSING_GOOG_REQUIRE


goog.scope(function() {
var unused = goog.events.unused; // UNUSED_LOCAL_VARIABLE
var used = goog.events.used; // ALIAS_STMT_NEEDS_GOOG_REQUIRE
var UsedType = goog.sample.UsedType;
var other = goog.sample.other;


/** @type {used.UsedAlias|other.UsedAlias} */
goog.something.Else = UsedType.create();
});  // goog.scope
