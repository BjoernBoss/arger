/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"

namespace arger {
	/* representation of a single argument value (performs primitive type-conversions when accessing values) */
	struct Value : private std::variant<uint64_t, int64_t, double, bool, std::wstring, arger::EnumValue> {
	private:
		using Parent = std::variant<uint64_t, int64_t, double, bool, std::wstring, arger::EnumValue>;

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
		constexpr Value(std::wstring&& v) : Parent{ std::move(v) } {}
		constexpr Value(const std::wstring& v) : Parent{ v } {}
		constexpr Value(arger::EnumValue&& v) : Parent{ std::move(v) } {}
		constexpr Value(const arger::EnumValue& v) : Parent{ v } {}

	public:
		/* convenience */
		constexpr Value(int v) : Parent{ int64_t(v) } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(unsigned int v) : Parent{ uint64_t(v) } {}
		constexpr Value(long v) : Parent{ int64_t(v) } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(unsigned long v) : Parent{ uint64_t(v) } {}
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
		template <arger::IsEnum Type>
		constexpr Type asEnum() const {
			if (std::holds_alternative<arger::EnumValue>(*this))
				return static_cast<Type>(std::get<arger::EnumValue>(*this).id);
			throw arger::TypeException{ L"arger::Value is not an enum." };
		}
		constexpr uint64_t unum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return std::get<uint64_t>(*this);
			if (std::holds_alternative<arger::EnumValue>(*this))
				return std::get<arger::EnumValue>(*this).id;
			throw arger::TypeException{ L"arger::Value is not an unsigned-number." };
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			if (std::holds_alternative<arger::EnumValue>(*this))
				return std::get<arger::EnumValue>(*this).id;
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
		constexpr const std::wstring& str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return std::get<std::wstring>(*this);
			if (std::holds_alternative<arger::EnumValue>(*this))
				return std::get<arger::EnumValue>(*this).name;
			throw arger::TypeException{ L"arger::Value is not a string." };
		}
	};
}
