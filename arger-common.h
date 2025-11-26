/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include <ustring/ustring.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <cwctype>
#include <variant>
#include <optional>
#include <set>
#include <type_traits>
#include <concepts>
#include <cinttypes>
#include <algorithm>
#include <typeindex>

namespace arger {
	class Parsed;
	namespace detail {
		class Parser;

		struct EnumId {
			size_t id = 0;
		};
	}

	/* any enum or integer is considered an id */
	template <class Type>
	concept IsId = std::is_integral_v<Type> || std::is_enum_v<Type>;

	enum class Primitive : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	struct EnumEntry {
		std::wstring name;
		std::wstring description;
		size_t id = 0;
		constexpr EnumEntry(std::wstring name, std::wstring description, arger::IsId auto id) : name{ name }, description{ description }, id{ static_cast<size_t>(id) } {}
	};
	using Enum = std::vector<arger::EnumEntry>;
	using Type = std::variant<arger::Primitive, arger::Enum>;

	using Checker = std::function<std::wstring(const arger::Parsed&)>;

	/* exception thrown when using the library in an invalid way */
	struct Exception : public str::BuildException {
		template <class... Args>
		constexpr Exception(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when a malformed argument-configuration is used */
	struct ConfigException : public arger::Exception {
		template <class... Args>
		constexpr ConfigException(const Args&... args) : arger::Exception{ args... } {}
	};

	/* exception thrown when accessing an arger::Value as a certain type, which it is not */
	struct TypeException : public arger::Exception {
		template <class... Args>
		constexpr TypeException(const Args&... args) : arger::Exception{ args... } {}
	};

	/* exception thrown when malformed or invalid arguments are encountered */
	struct ParsingException : public arger::Exception {
		template <class... Args>
		constexpr ParsingException(const Args&... args) : arger::Exception{ args... } {}
	};

	/* exception thrown when only a message should be printed but no */
	struct PrintMessage : public arger::Exception {
		template <class... Args>
		constexpr PrintMessage(const Args&... args) : arger::Exception{ args... } {}
	};
}
