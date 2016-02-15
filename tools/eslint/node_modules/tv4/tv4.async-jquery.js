// Provides support for asynchronous validation (fetching schemas) using jQuery
// Callback is optional third argument to tv4.validate() - if not present, synchronous operation
//     callback(result, error);
if (typeof (tv4.asyncValidate) === 'undefined') {
	tv4.syncValidate = tv4.validate;
	tv4.validate = function (data, schema, callback, checkRecursive, banUnknownProperties) {
		if (typeof (callback) === 'undefined') {
			return this.syncValidate(data, schema, checkRecursive, banUnknownProperties);
		} else {
			return this.asyncValidate(data, schema, callback, checkRecursive, banUnknownProperties);
		}
	};
	tv4.asyncValidate = function (data, schema, callback, checkRecursive, banUnknownProperties) {
		var $ = jQuery;
		var result = tv4.validate(data, schema, checkRecursive, banUnknownProperties);
		if (!tv4.missing.length) {
			callback(result, tv4.error);
		} else {
			// Make a request for each missing schema
			var missingSchemas = $.map(tv4.missing, function (schemaUri) {
				return $.getJSON(schemaUri).success(function (fetchedSchema) {
					tv4.addSchema(schemaUri, fetchedSchema);
				}).error(function () {
					// If there's an error, just use an empty schema
					tv4.addSchema(schemaUri, {});
				});
			});
			// When all requests done, try again
			$.when.apply($, missingSchemas).done(function () {
				var result = tv4.asyncValidate(data, schema, callback, checkRecursive, banUnknownProperties);
			});
		}
	};
}
