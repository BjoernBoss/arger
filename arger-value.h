/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"

namespace arger {
	namespace detail {
		struct EnumId {
			size_t id = 0;
		};
	}

	/* representation of a single argument value (performs primitive type-conversions when accessing values) */
	struct Value : private std::variant<uint64_t, int64_t, double, bool, std::wstring, detail::EnumId> {
	private:
		using Parent = std::variant<uint64_t, int64_t, double, bool, std::wstring, detail::EnumId>;

	public:
		Value() : Parent{ 0llu } {}
		Value(arger::Value&&) = default;
		Value(const arger::Value&) = default;
		arger::Value& operator=(arger::Value&&) = default;
		arger::Value& operator=(const arger::Value&) = default;

	public:
		constexpr Value(uint64_t v) : Parent{ v } {}
		constexpr Value(int64_t v) : Parent{ v } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(double v) : Parent{ v } {}
		constexpr Value(bool v) : Parent{ v } {}
		constexpr Value(const str::IsStr auto& v) : Parent{ str::wd::To(v) } {}
		constexpr Value(const detail::EnumId& v) : Parent{ v } {}

	public:
		/* convenience */
		constexpr Value(int v) : Parent{ int64_t(v) } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(unsigned int v) : Parent{ uint64_t(v) } {}

	public:
		constexpr bool isId() const {
			return std::holds_alternative<detail::EnumId>(*this);
		}
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
		template <arger::IsId Type = size_t>
		constexpr Type id() const {
			if (std::holds_alternative<detail::EnumId>(*this))
				return static_cast<Type>(std::get<detail::EnumId>(*this).id);
			throw arger::TypeException{ L"arger::Value is not an enum." };
		}
		constexpr uint64_t unum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return std::get<uint64_t>(*this);
			throw arger::TypeException{ L"arger::Value is not an unsigned-number." };
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			throw arger::TypeException{ L"arger::Value is not a signed-number." };
		}
		constexpr double real() const {
			if (std::holds_alternative<double>(*this))
				return std::get<double>(*this);
			if (std::holds_alternative<uint64_t>(*this))
				return double(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return double(std::get<int64_t>(*this));
			throw arger::TypeException{ L"arger::Value is not a real." };
		}
		constexpr bool boolean() const {
			if (std::holds_alternative<bool>(*this))
				return std::get<bool>(*this);
			throw arger::TypeException{ L"arger::Value is not a boolean." };
		}
		template <str::IsChar ChType = wchar_t>
		constexpr std::basic_string<ChType> str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return str::View{ std::get<std::wstring>(*this) }.str<ChType>();
			throw arger::TypeException{ L"arger::Value is not a string." };
		}
	};
}
