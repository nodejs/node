/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Nihilai Collective Corp
 * https://github.com/nihilai-collective/jsonifier
 * include/jsonifier-incl/utilities/concepts.hpp
 */
#pragma once

#include <jsonifier-incl/utilities/forward.hpp>

namespace jsonifier {

	enum class json_types : uint8_t {
		object	= 0,
		array	= 1,
		string	= 2,
		number	= 3,
		boolean = 4,
		null	= 5,
		unset	= 8,
		count,
	};

	class string_view_base;

	template<uint64_t = 0> class string_base;

	class raw_json_data;

	// Idea for this interface sampled from Stephen Berry and his library, Glaze library: https://github.com/stephenberry/glaze
	template<typename value_type> struct core;

	struct skip {};

}

namespace jsonifier::internal {

	template<typename context_type>
	concept structural_context_types = std::same_as<typename context_type::template_iterator_type, unrestrict_t<write_structural_index_ptr>>;

	template<typename context_type> using context_iterator_t = typename context_type::iterator_type;

	template<typename value_type>
	concept simd_int_512_types = sizeof(value_type) == 64;
	template<typename value_type>
	concept simd_int_256_types = sizeof(value_type) == 32;
	template<typename value_type>
	concept simd_int_128_types = sizeof(value_type) == 16;

#if JSONIFIER_SVE2
	template<typename value_type>
	concept simd_int_sve2_types = std::same_as<remove_cvref_t<value_type>, jsonifier_simd_int_t>;
#endif

	template<typename value_type>
	concept json_entity_types = requires {
		typename value_type::class_type;
		value_type::memberPtr;
	};

	template<typename value_type>
	concept json_entity_convertible_types = json_entity_types<value_type> || std::is_member_pointer_v<value_type>;

	template<typename value_type> using base_t = remove_cvref_t<value_type>;

	template<typename, typename> inline constexpr bool isSame						   = false;
	template<typename value_type> inline constexpr bool isSame<value_type, value_type> = true;

	template<typename value_type>
	concept static_sized_types = requires() { base_t<value_type>::staticSize; };

	template<typename value_type_01, typename value_type_02>
	concept same_as_types = isSame<remove_reference_t<value_type_01>, remove_reference_t<value_type_02>>;

	template<typename value_type>
	concept enum_types = std::is_enum_v<base_t<value_type>> && std::is_unsigned_v<std::underlying_type_t<base_t<value_type>>>;

	template<typename value_type>
	concept integral_types = std::is_integral_v<base_t<value_type>> && !same_as_types<base_t<value_type>, bool>;

	template<typename value_type>
	concept trivially_copyable_types = __is_trivial(base_t<value_type>) || std::is_standard_layout_v<base_t<value_type>>;

	template<typename value_type>
	concept trivially_destructible_types = std::is_trivially_destructible_v<base_t<value_type>>;

	template<typename value_type>
	concept trivially_constructible_types = std::is_trivially_constructible_v<base_t<value_type>>;

	template<typename value_type>
	concept trivially_constructible_copyable_destructible_types =
		trivially_constructible_types<value_type> && trivially_copyable_types<value_type> && trivially_destructible_types<value_type>;

	template<typename value_type>
	concept int_types = std::is_signed_v<base_t<value_type>> && integral_types<value_type>;

	template<typename value_type>
	concept int8_types = int_types<value_type> && sizeof(remove_cvref_t<value_type>) == 1;

	template<typename value_type>
	concept int16_types = int_types<value_type> && sizeof(remove_cvref_t<value_type>) == 2;

	template<typename value_type>
	concept int32_types = int_types<value_type> && sizeof(remove_cvref_t<value_type>) == 4;

	template<typename value_type>
	concept int64_types = int_types<value_type> && sizeof(remove_cvref_t<value_type>) == 8;

	template<typename value_type>
	concept uint_types = std::is_unsigned_v<base_t<value_type>> && integral_types<value_type>;

	template<typename value_type>
	concept uint8_types = uint_types<value_type> && sizeof(remove_cvref_t<value_type>) == 1;

	template<typename value_type>
	concept uint16_types = uint_types<value_type> && sizeof(remove_cvref_t<value_type>) == 2;

	template<typename value_type>
	concept uint32_types = uint_types<value_type> && sizeof(remove_cvref_t<value_type>) == 4;

	template<typename value_type>
	concept uint64_types = uint_types<value_type> && sizeof(remove_cvref_t<value_type>) == 8;

	template<typename value_type>
	concept float_types = std::floating_point<base_t<value_type>> && (std::numeric_limits<base_t<value_type>>::radix == 2) && std::numeric_limits<base_t<value_type>>::is_iec559;

	template<typename value_type>
	concept float64_types = float_types<value_type> && sizeof(remove_cvref_t<value_type>) == 8;

	template<typename value_type>
	concept float32_types = float_types<value_type> && sizeof(remove_cvref_t<value_type>) == 4;

	template<typename value_type>
	concept string_view_convertible_types = std::convertible_to<base_t<value_type>, std::string_view>;

	template<typename value_type_01, typename value_type_02>
	concept at_least_one_enum_types = ( enum_types<value_type_01> && uint_types<value_type_02> ) || (uint_types<value_type_01> && enum_types<value_type_02>);

	template<typename value_type_01, typename value_type_02>
	concept indexable_types =
		at_least_one_enum_types<value_type_01, value_type_02> || same_as_types<value_type_01, value_type_02> || (integral_types<value_type_01> && integral_types<value_type_02>);

	template<typename value_type>
	concept resize_and_overwriteable_types = requires(value_type value) {
		value.resize_and_overwrite(uint64_t{}, [](typename value_type::pointer, uint64_t) noexcept {
			return uint64_t{};
		});
	};

	template<typename value_type>
	concept skip_types = std::is_same_v<base_t<value_type>, skip>;

	template<typename value_type>
	concept iterable_types = requires(base_t<value_type> value) {
		{ value.begin() };
		{ value.end() };
	};

	template<typename value_type>
	concept map_subscriptable_types = requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::key_type>()] } -> std::same_as<const typename base_t<value_type>::mapped_type&>;
	} || requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::key_type>()] } -> std::same_as<typename base_t<value_type>::mapped_type&>;
	};

	template<typename value_type>
	concept vector_subscriptable_types = requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::size_type>()] } -> std::same_as<typename base_t<value_type>::const_reference>;
	} || requires(base_t<value_type> value) {
		{ value[std::declval<typename base_t<value_type>::size_type>()] } -> std::same_as<typename base_t<value_type>::reference>;
	};

	template<typename value_type>
	concept sized_types = requires(base_t<value_type> value) {
		{ value.size() } -> std::same_as<typename base_t<value_type>::size_type>;
	};

	template<typename value_type>
	concept empty_checkable_types = requires(base_t<value_type> value) {
		{ value.empty() } -> std::same_as<bool>;
	};

	template<typename value_type>
	concept variant_types = requires(base_t<value_type> var) {
		{ var.index() } -> std::convertible_to<uint64_t>;
		{ var.valueless_by_exception() } -> std::same_as<bool>;
		{ std::holds_alternative<decltype(std::get<0>(var))>(var) } -> std::same_as<bool>;
		{ std::get<0>(var) } -> std::same_as<decltype(std::get<0>(var))&>;
		{ std::get_if<0>(&var) } -> std::same_as<base_t<decltype(std::get<0>(var))>*>;
	};

	template<typename value_type>
	concept resizeable_types = requires(base_t<value_type> value) { value.resize(std::declval<typename base_t<value_type>::size_type>()); };

	template<typename value_type>
	concept reserveable_types = requires(base_t<value_type> value) { value.reserve(std::declval<typename base_t<value_type>::size_type>()); };

	template<typename value_type>
	concept data_accessible_types = requires(base_t<value_type> value) {
		{ value.data() };
	};

	template<typename value_type>
	concept bool_types = std::same_as<base_t<value_type>, bool> || std::same_as<base_t<value_type>, std::vector<bool>::reference> ||
		std::same_as<base_t<value_type>, std::vector<bool>::const_reference>;

	template<typename value_type>
	concept always_null_types =
		std::same_as<base_t<value_type>, std::nullptr_t> || std::same_as<base_t<value_type>, std::monostate> || std::same_as<base_t<value_type>, std::nullopt_t>;

	template<typename value_type>
	concept pointer_types =
		(std::is_pointer_v<base_t<value_type>> || ( std::is_null_pointer_v<base_t<value_type>> && !std::is_array_v<base_t<value_type>> )) && !always_null_types<value_type>;

	template<typename value_type>
	concept char_types = std::same_as<base_t<value_type>, char> || std::same_as<base_t<value_type>, wchar_t> || std::same_as<base_t<value_type>, char8_t> ||
		std::same_as<base_t<value_type>, char16_t> || std::same_as<base_t<value_type>, char32_t>;

	template<typename value_type>
	concept number_types = (float_types<value_type> || uint_types<value_type> || int_types<value_type>) && !char_types<value_type>;

	template<typename value_type>
	concept substringable_types = requires(base_t<value_type> value) {
		{ value.substr(std::declval<typename base_t<value_type>::size_type>(), std::declval<typename base_t<value_type>::size_type>()) } -> std::same_as<base_t<value_type>>;
	};

	template<typename value_type>
	concept findable_types = requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::size_type>;
	} || requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::key_type>()) } -> std::same_as<typename base_t<value_type>::iterator>;
	} || requires(base_t<value_type> value) {
		{ value.find(std::declval<typename base_t<value_type>::key_type>()) } -> std::same_as<typename base_t<value_type>::const_iterator>;
	};

	template<typename value_type>
	concept string_types =
		substringable_types<value_type> && data_accessible_types<value_type> && sized_types<value_type> && vector_subscriptable_types<value_type> && findable_types<value_type>;

	template<typename value_type>
	concept string_view_types = substringable_types<value_type> && data_accessible_types<value_type> && sized_types<value_type> && vector_subscriptable_types<value_type> &&
		findable_types<value_type> && !resizeable_types<value_type>;

	template<typename value_type>
	concept map_types =
		map_subscriptable_types<value_type> && iterable_types<value_type> && sized_types<value_type> && findable_types<value_type> && empty_checkable_types<value_type>;

	template<typename value_type>
	concept emplace_backable_types = requires(base_t<value_type> value) {
		{ value.emplace_back(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::reference>;
	};

	template<typename value_type>
	concept releasable_types = requires(base_t<value_type> value) {
		{ value.release() } -> std::same_as<typename base_t<value_type>::pointer>;
	};

	template<typename value_type>
	concept resettable_types = requires(base_t<value_type> value) {
		{ value.reset() } -> std::same_as<void>;
	};

	template<typename value_type>
	concept gettable_types = requires(base_t<value_type> value) {
		{ value.get() } -> std::same_as<typename base_t<value_type>::element_type*>;
	};

	template<typename value_type>
	concept copyable_types = std::copyable<base_t<value_type>>;

	template<typename value_type>
	concept unique_ptr_types = requires(base_t<value_type> value) {
		typename base_t<value_type>::element_type;
		typename base_t<value_type>::deleter_type;
	} && releasable_types<value_type> && gettable_types<value_type>;

	template<typename value_type>
	concept shared_ptr_types = resettable_types<value_type> && gettable_types<value_type> && copyable_types<value_type>;

	template<typename value_type>
	concept any_pointer_types = unique_ptr_types<value_type> || shared_ptr_types<value_type> || pointer_types<value_type>;

	template<typename value_type>
	concept excluded_keys_types = requires(base_t<value_type> value) {
		{ value.jsonifierExcludedKeys };
	};

	template<typename value_type>
	concept raw_json_types = std::same_as<base_t<value_type>, raw_json_data>;

	template<typename value_type01, typename value_type02>
	concept same_character_size_types = requires { sizeof(typename base_t<value_type01>::value_type) == sizeof(typename base_t<value_type02>::value_type); } &&
		string_types<value_type01> && string_types<value_type02>;

	template<typename value_type> inline constexpr bool hasSizeEqualToZero{ std::tuple_size_v<base_t<value_type>> == 0 };

	template<typename value_type>
	concept skip_or_always_null_types = skip_types<value_type> || always_null_types<value_type>;

	template<typename value_type>
	concept tuple_types = requires { std::tuple_size<base_t<value_type>>::value; } && !data_accessible_types<value_type>;

	template<typename value_type> using decay_keep_volatile_t = std::remove_const_t<remove_reference_t<value_type>>;

	template<typename value_type>
	concept optional_types = requires(base_t<value_type> opt) {
		{ opt.has_value() } -> std::same_as<bool>;
		{ opt.value() } -> std::same_as<typename base_t<value_type>::value_type&>;
		{ *opt } -> std::same_as<typename base_t<value_type>::value_type&>;
		{ opt.reset() } -> std::same_as<void>;
		{ opt.emplace(std::declval<typename base_t<value_type>::value_type>()) } -> std::same_as<typename base_t<value_type>::value_type&>;
	};

	template<typename value_type>
	concept any_pointer_or_optional_types = any_pointer_types<value_type> || optional_types<value_type>;

	template<typename value_type>
	concept any_enum_types = std::is_enum_v<base_t<value_type>>;

	template<typename value_type>
	concept vector_types = vector_subscriptable_types<value_type> && resizeable_types<value_type> && emplace_backable_types<value_type>;

	template<typename value_type>
	concept float_vector_types = vector_types<value_type> && float_types<typename base_t<value_type>::value_type>;

	template<typename value_type>
	concept jsonifier_object_types = requires { core<base_t<value_type>>::parseValue; };

	template<typename value_type>
	concept raw_array_types = ( std::is_array_v<base_t<value_type>> && !std::is_pointer_v<base_t<value_type>> ) ||
		(vector_subscriptable_types<value_type> && !vector_types<value_type> && !substringable_types<value_type> && !tuple_types<value_type>);

	template<typename value_type>
	concept buffer_like_types = vector_subscriptable_types<value_type> && data_accessible_types<value_type> && resizeable_types<value_type>;

	template<typename value_type>
	concept integer_types = std::integral<base_t<value_type>> && !bool_types<value_type> && !std::floating_point<base_t<value_type>>;

	template<typename value_type>
	concept count_types = requires { base_t<value_type>::count; } && static_cast<uint64_t>(base_t<value_type>::count) < 128;

	template<typename value_type>
	concept printable_enum_types = enum_types<value_type> && count_types<value_type>;

	template<typename value_type>
	concept integral_constant_types = requires {
		{ base_t<value_type>::value } -> std::convertible_to<uint64_t>;
	};

	template<typename value_type>
	concept eq_0_types = base_t<value_type>::length == 0;

	template<typename value_type>
	concept gt_0_lte_8_types = base_t<value_type>::length > 0 && base_t<value_type>::length <= 8;

	template<typename value_type>
	concept gt_8_types = base_t<value_type>::length > 8;

	template<typename value_type>
	concept eq_1_types = base_t<value_type>::length == 1;

	template<typename value_type>
	concept eq_2_types = base_t<value_type>::length == 2;

	template<typename value_type>
	concept eq_3_types = base_t<value_type>::length == 3;

	template<typename value_type>
	concept eq_4_types = base_t<value_type>::length == 4;

	template<typename value_type>
	concept eq_5_types = base_t<value_type>::length == 5;

	template<typename value_type>
	concept eq_6_types = base_t<value_type>::length == 6;

	template<typename value_type>
	concept eq_7_types = base_t<value_type>::length == 7;

	template<typename value_type>
	concept eq_8_types = base_t<value_type>::length == 8;

	template<typename value_type>
	concept gt_8_lt_16_types = base_t<value_type>::length > 8 && base_t<value_type>::length < 16;

	template<typename value_type>
	concept eq_16_types = base_t<value_type>::length == 16 && simdBytesPerRegister >= 16;

	template<typename value_type>
	concept eq_32_types = base_t<value_type>::length == 32 && simdBytesPerRegister >= 32;

	template<typename value_type>
	concept eq_64_types = base_t<value_type>::length == 64 && simdBytesPerRegister >= 64;

	template<typename value_type>
	concept gt_16_types = base_t<value_type>::length > 16 && !eq_16_types<value_type> && !eq_32_types<value_type> && !eq_64_types<value_type>;

	template<typename value_type>
	concept gt_16_lt_32_types = gt_16_types<value_type> && base_t<value_type>::length < 32;

	template<typename value_type>
	concept gt_32_lt_64_types = gt_16_types<value_type> && base_t<value_type>::length > 32 && base_t<value_type>::length < 64;

	template<typename value_type>
	concept named_types = requires(base_t<value_type> value) { value.name; };

	template<typename value_type>
	concept derivable_types = std::is_class_v<value_type> && !std::is_final_v<value_type>;

	template<typename value_type_01, typename value_type_02>
	concept eq_comparable_types = requires { std::declval<value_type_01>() == std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept neq_comparable_types = requires { std::declval<value_type_01>() != std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept lt_comparable_types = requires { std::declval<value_type_01>() < std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept lte_comparable_types = requires { std::declval<value_type_01>() <= std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept gt_comparable_types = requires { std::declval<value_type_01>() > std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept gte_comparable_types = requires { std::declval<value_type_01>() >= std::declval<value_type_02>(); };

	template<typename value_type_01, typename value_type_02>
	concept ss_comparable_types = requires { std::declval<value_type_01>() <=> std::declval<value_type_02>(); };

	template<typename value_type, template<typename...> typename template_type> struct is_specialization_of_impl : std::false_type {};

	template<template<typename...> typename template_type, typename... args> struct is_specialization_of_impl<template_type<args...>, template_type> : std::true_type {};

	template<typename value_type, template<typename...> typename template_type>
	concept specialization_of_types = is_specialization_of_impl<value_type, template_type>::value;

	template<typename value_type>
	concept prettify_buffer_types = string_types<value_type> || std::is_convertible_v<value_type, write_buffer_ptr>;

}
