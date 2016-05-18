(function (global) {
	var lang = {
		INVALID_TYPE: "Ungültiger Typ: {type} (erwartet wurde: {expected})",
		ENUM_MISMATCH: "Keine Übereinstimmung mit der Aufzählung (enum) für: {value}",
		ANY_OF_MISSING: "Daten stimmen nicht überein mit einem der Schemas von \"anyOf\"",
		ONE_OF_MISSING: "Daten stimmen nicht überein mit einem der Schemas von \"oneOf\"",
		ONE_OF_MULTIPLE: "Daten sind valid in Bezug auf mehreren Schemas von \"oneOf\": index {index1} und {index2}",
		NOT_PASSED: "Daten stimmen mit dem \"not\" Schema überein",
		// Numeric errors
		NUMBER_MULTIPLE_OF: "Wert {value} ist kein Vielfaches von {multipleOf}",
		NUMBER_MINIMUM: "Wert {value} ist kleiner als das Minimum {minimum}",
		NUMBER_MINIMUM_EXCLUSIVE: "Wert {value} ist gleich dem Exklusiven Minimum {minimum}",
		NUMBER_MAXIMUM: "Wert {value} ist größer als das Maximum {maximum}",
		NUMBER_MAXIMUM_EXCLUSIVE: "Wert {value} ist gleich dem Exklusiven Maximum {maximum}",
		// String errors
		STRING_LENGTH_SHORT: "Zeichenkette zu kurz ({length} chars), minimum {minimum}",
		STRING_LENGTH_LONG: "Zeichenkette zu lang ({length} chars), maximum {maximum}",
		STRING_PATTERN: "Zeichenkette entspricht nicht dem Muster: {pattern}",
		// Object errors
		OBJECT_PROPERTIES_MINIMUM: "Zu wenige Attribute definiert ({propertyCount}), minimum {minimum}",
		OBJECT_PROPERTIES_MAXIMUM: "Zu viele Attribute definiert ({propertyCount}), maximum {maximum}",
		OBJECT_REQUIRED: "Notwendiges Attribut fehlt: {key}",
		OBJECT_ADDITIONAL_PROPERTIES: "Zusätzliche Attribute nicht erlaubt",
		OBJECT_DEPENDENCY_KEY: "Abhängigkeit fehlt - Schlüssel nicht vorhanden: {missing} (wegen Schlüssel: {key})",
		// Array errors
		ARRAY_LENGTH_SHORT: "Array zu kurz ({length}), minimum {minimum}",
		ARRAY_LENGTH_LONG: "Array zu lang ({length}), maximum {maximum}",
		ARRAY_UNIQUE: "Array Einträge nicht eindeutig (Index {match1} und {match2})",
		ARRAY_ADDITIONAL_ITEMS: "Zusätzliche Einträge nicht erlaubt"
	};

	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('de', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports){
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('de', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('de', lang);
	}
})(this);
