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
		std::set<size_t> pFlags;
		std::map<size_t, std::vector<arger::Value>> pOptions;
		std::vector<arger::Value> pPositional;
		size_t pGroupId = 0;

	public:
		bool flag(arger::IsId auto id) const {
			return pFlags.contains(static_cast<size_t>(id));
		}
		template <arger::IsId Type = size_t>
		constexpr Type id() const {
			return static_cast<Type>(pGroupId);
		}

	public:
		size_t options(arger::IsId auto id) const {
			auto it = pOptions.find(static_cast<size_t>(id));
			return (it == pOptions.end() ? 0 : it->second.size());
		}
		std::optional<arger::Value> option(arger::IsId auto id, size_t index = 0) const {
			auto it = pOptions.find(static_cast<size_t>(id));
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
