/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-value.h"

namespace arger {
	/* represents the parsed results of the arguments */
	class Parsed {
		friend class arger::Arguments;
		friend class detail::Parser;
	private:
		std::set<std::wstring> pFlags;
		std::map<std::wstring, std::vector<arger::Value>> pOptions;
		std::vector<arger::Value> pPositional;
		size_t pGroupId = 0;

	public:
		bool flag(const std::wstring& name) const {
			return pFlags.contains(name);
		}
		constexpr size_t id() const {
			return pGroupId;
		}
		template <arger::IsEnum Type>
		constexpr Type idAsEnum() const {
			return static_cast<Type>(pGroupId);
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
		std::optional<arger::Value> positional(size_t index) const {
			if (index >= pPositional.size())
				return {};
			return pPositional[index];
		}
	};
}
