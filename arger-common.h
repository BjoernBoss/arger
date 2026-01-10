/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2026 Bjoern Boss Henrichsen */
#pragma once

#include <ustring/ustring.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <list>
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

	/* exception thrown when using the library in an invalid way */
	struct Exception : public str::ch::BuildException {
		template <class... Args>
		constexpr Exception(const Args&... args) : str::ch::BuildException{ args... } {}
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
