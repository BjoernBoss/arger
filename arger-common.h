/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
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

namespace arger {
	class Parsed;
	class Arguments;
	namespace detail {
		class Parser;
	}

	enum class Primitive : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	using Enum = std::map<std::wstring, std::wstring>;
	using Type = std::variant<arger::Primitive, arger::Enum>;

	using Checker = std::function<std::wstring(const arger::Parsed&)>;

	/* exception thrown when a malformed argument-configuration is used */
	struct ConfigException : public str::BuildException {
		template <class... Args>
		constexpr ConfigException(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when accessing an arger::Value as a certain type, which it is not */
	struct TypeException : public str::BuildException {
		template <class... Args>
		constexpr TypeException(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when malformed or invalid arguments are encountered */
	struct ParsingException : public str::BuildException {
		template <class... Args>
		constexpr ParsingException(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when only a message should be printed but no */
	struct PrintMessage : public str::BuildException {
		template <class... Args>
		constexpr PrintMessage(const Args&... args) : str::BuildException{ args... } {}
	};
}
