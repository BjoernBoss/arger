/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"

namespace arger {
	namespace detail {
		class Parser;
	}

	/* representation of a single argument value (performs primitive type-conversions when accessing values) */
	struct Value : private std::variant<uint64_t, int64_t, double, bool, std::wstring> {
	private:
		using Parent = std::variant<uint64_t, int64_t, double, bool, std::wstring>;

	public:
		constexpr Value() : Parent{ 0llu } {}
		constexpr Value(arger::Value&&) = default;
		constexpr Value(const arger::Value&) = default;
		constexpr arger::Value& operator=(arger::Value&&) = default;
		constexpr arger::Value& operator=(const arger::Value&) = default;

	public:
		constexpr Value(uint64_t v) : Parent{ v } {}
		constexpr Value(int64_t v) : Parent{ v } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(double v) : Parent{ v } {}
		constexpr Value(bool v) : Parent{ v } {}
		constexpr Value(std::wstring&& v) : Parent{ std::move(v) } {}
		constexpr Value(const std::wstring& v) : Parent{ v } {}

	public:
		/* convenience */
		constexpr Value(int v) : Parent{ int64_t(v) } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(const wchar_t* s) : Parent{ std::wstring(s) } {}

	public:
		constexpr bool isUNum() const {
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isINum() const {
			if (std::holds_alternative<int64_t>(*this))
				return true;
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isReal() const {
			if (std::holds_alternative<double>(*this))
				return true;
			if (std::holds_alternative<int64_t>(*this))
				return true;
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isBool() const {
			return std::holds_alternative<bool>(*this);
		}
		constexpr bool isStr() const {
			return std::holds_alternative<std::wstring>(*this);
		}

	public:
		constexpr uint64_t unum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return std::get<uint64_t>(*this);
			throw arger::TypeException(L"arger::Value is not an unsigned-number");
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			throw arger::TypeException(L"arger::Value is not a signed-number");
		}
		constexpr double real() const {
			if (std::holds_alternative<double>(*this))
				return std::get<double>(*this);
			if (std::holds_alternative<uint64_t>(*this))
				return double(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return double(std::get<int64_t>(*this));
			throw arger::TypeException(L"arger::Value is not a real");
		}
		constexpr bool boolean() const {
			if (std::holds_alternative<bool>(*this))
				return std::get<bool>(*this);
			throw arger::TypeException(L"arger::Value is not a boolean");
		}
		constexpr const std::wstring& str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return std::get<std::wstring>(*this);
			throw arger::TypeException(L"arger::Value is not a string");
		}
	};

	/* represents the parsed results of the arguments */
	class Parsed {
		friend class arger::Arguments;
		friend class detail::Parser;
	private:
		std::set<std::wstring> pFlags;
		std::map<std::wstring, std::vector<arger::Value>> pOptions;
		std::vector<arger::Value> pPositional;
		std::wstring pGroupId;

	public:
		bool flag(const std::wstring& name) const {
			return pFlags.contains(name);
		}
		constexpr const std::wstring& groupId() const {
			return pGroupId;
		}

	public:
		size_t options(const std::wstring& name) const {
			auto it = pOptions.find(name);
			return (it == pOptions.end() ? 0 : it->second.size());
		}
		std::optional<arger::Value> option(const std::wstring& name, size_t index = 0) const {
			auto it = pOptions.find(name);
			if (it == pOptions.end() || index >= it->second.size())
				return {};
			return it->second[index];
		}
		constexpr size_t positionals() const {
			return pPositional.size();
		}
		constexpr std::optional<arger::Value> positional(size_t index) const {
			if (index >= pPositional.size())
				return {};
			return pPositional[index];
		}
	};
}
