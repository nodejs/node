(function (global) {
	var lang = {
		INVALID_TYPE: "当前类型 {type} 不符合期望的类型 {expected}",
		ENUM_MISMATCH: "{value} 不是有效的枚举类型取值",
		ANY_OF_MISSING: "数据不符合以下任何一个模式 (\"anyOf\")",
		ONE_OF_MISSING: "数据不符合以下任何一个模式 (\"oneOf\")",
		ONE_OF_MULTIPLE: "数据同时符合多个模式 (\"oneOf\"): 下标 {index1} 和 {index2}",
		NOT_PASSED: "数据不应匹配以下模式 (\"not\")",
		// Numeric errors
		NUMBER_MULTIPLE_OF: "数值 {value} 不是 {multipleOf} 的倍数",
		NUMBER_MINIMUM: "数值 {value} 小于最小值 {minimum}",
		NUMBER_MINIMUM_EXCLUSIVE: "数值 {value} 等于排除的最小值 {minimum}",
		NUMBER_MAXIMUM: "数值 {value} is greater 大于最大值 {maximum}",
		NUMBER_MAXIMUM_EXCLUSIVE: "数值 {value} 等于排除的最大值 {maximum}",
		NUMBER_NOT_A_NUMBER: "数值 {value} 不是有效的数字",
		// String errors
		STRING_LENGTH_SHORT: "字符串太短 ({length} 个字符), 最少 {minimum} 个",
		STRING_LENGTH_LONG: "字符串太长 ({length} 个字符), 最多 {maximum} 个",
		STRING_PATTERN: "字符串不匹配模式: {pattern}",
		// Object errors
		OBJECT_PROPERTIES_MINIMUM: "字段数过少 ({propertyCount}), 最少 {minimum} 个",
		OBJECT_PROPERTIES_MAXIMUM: "字段数过多 ({propertyCount}), 最多 {maximum} 个",
		OBJECT_REQUIRED: "缺少必要字段: {key}",
		OBJECT_ADDITIONAL_PROPERTIES: "不允许多余的字段",
		OBJECT_DEPENDENCY_KEY: "依赖失败 - 缺少键 {missing} (来自键: {key})",
		// Array errors
		ARRAY_LENGTH_SHORT: "数组长度太短 ({length}), 最小长度 {minimum}",
		ARRAY_LENGTH_LONG: "数组长度太长 ({length}), 最大长度 {maximum}",
		ARRAY_UNIQUE: "数组元素不唯一 (下标 {match1} 和 {match2})",
		ARRAY_ADDITIONAL_ITEMS: "不允许多余的元素",
		// Format errors
		FORMAT_CUSTOM: "格式校验失败 ({message})",
		KEYWORD_CUSTOM: "关键字 {key} 校验失败: ({message})",
		// Schema structure
		CIRCULAR_REFERENCE: "循环引用 ($refs): {urls}",
		// Non-standard validation options
		UNKNOWN_PROPERTY: "未知字段 (不在 schema 中)"
	};

	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('zh-CN', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports){
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('zh-CN', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('zh-CN', lang);
	}
})(this);
