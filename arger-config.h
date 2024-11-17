/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"

namespace arger {
	namespace detail {
		struct Positional {
			std::wstring name;
			arger::Type type;
			std::wstring description;
		};
		struct Help {
			std::wstring name;
			std::wstring description;
		};
	}

	struct Configuration {
	public:
		std::set<std::wstring> users;
		std::vector<arger::Constraint> constraints;
		std::vector<detail::Positional> positional;
		std::vector<detail::Help> help;
		std::wstring description;
		std::wstring payload;
		arger::Type type;
		size_t minimum = 0;
		size_t maximum = 0;
		wchar_t abbreviation = 0;
		bool helpFlag = false;
		bool versionFlag = false;
		bool minimumSet = false;
		bool maximumSet = false;

	public:
		template <class RType>
		constexpr arger::Configuration& operator|(const RType& r) {
			r.apply(*this);
			return *this;
		}
	};

	namespace detail {
		template <class Type>
		struct ConfigBase {
			constexpr operator arger::Configuration() const {
				arger::Configuration config;
				static_cast<const Type&>(*this).apply(config);
				return config;
			}
			template <class RType>
			constexpr arger::Configuration operator|(const RType& r) const {
				arger::Configuration config;
				static_cast<const Type&>(*this).apply(config);
				r.apply(config);
				return config;
			}
		};
	}

	namespace config {
		/* [global/options/groups] add description for the arguments/group/option */
		struct Description : public detail::ConfigBase<config::Description> {
			std::wstring desc;
			constexpr Description(std::wstring desc) : desc{ desc } {}
			constexpr void apply(arger::Configuration& config) const {
				config.description = desc;
			}
		};

		/* [options/groups] configure the minimum and maximum required counters (minimum greater than 0 implies required; maximum
		*	of 0 implies no maximum; default for options is [0; 1], default for groups is number of positional arguments) */
		struct Require : public detail::ConfigBase<config::Require> {
			size_t minimum = 0;
			size_t maximum = 0;
			bool minimumSet = false;
			bool maximumSet = false;
			constexpr Require(size_t min = 1) : minimum{ min }, minimumSet{ true } {}
			constexpr Require(size_t min, size_t max) : minimum{ min }, maximum{ max }, minimumSet{ true }, maximumSet{ true } {}
			constexpr void apply(arger::Configuration& config) const {
				config.minimum = minimum;
				config.minimumSet = minimumSet;
				config.maximum = maximum;
				config.maximumSet = maximumSet;
			}
		};

		/* [options] add a shortcut single character to reference this option */
		struct Abbreviation : public detail::ConfigBase<config::Abbreviation> {
			wchar_t chr;
			constexpr Abbreviation(wchar_t c) : chr{ c } {}
			constexpr void apply(arger::Configuration& config) const {
				config.abbreviation = chr;
			}
		};

		/* [options] add a payload requirement to the option */
		struct Payload : public detail::ConfigBase<config::Payload> {
			std::wstring name;
			arger::Type type;
			constexpr Payload(std::wstring name, arger::Type type) : name{ name }, type{ type } {}
			constexpr void apply(arger::Configuration& config) const {
				config.payload = name;
				config.type = type;
			}
		};

		/* [options] bind the option to a subset of defined groups */
		struct Bind : public detail::ConfigBase<config::Bind> {
			std::set<std::wstring> users;
			Bind(std::set<std::wstring> groups) : users{ groups } {}
			void apply(arger::Configuration& config) const {
				config.users.insert(users.begin(), users.end());
			}
		};

		/* [global/options/groups] define callback-constraints to be executed (all globals or) if one or more of the groups/options have been passed in */
		struct Constraint : public detail::ConfigBase<config::Constraint> {
			arger::Constraint constraint;
			Constraint(arger::Constraint con) : constraint{ con } {}
			constexpr void apply(arger::Configuration& config) const {
				config.constraints.push_back(constraint);
			}
		};

		/* [options] define this as the help-menu option-flag */
		struct HelpFlag : detail::ConfigBase<config::HelpFlag> {
			constexpr HelpFlag() {}
			constexpr void apply(arger::Configuration& config) const {
				config.helpFlag = true;
			}
		};

		/* [options] define this as the version-menu option-flag */
		struct VersionFlag : detail::ConfigBase<config::VersionFlag> {
			constexpr VersionFlag() {}
			constexpr void apply(arger::Configuration& config) const {
				config.versionFlag = true;
			}
		};

		/* [groups] add another positional argument */
		struct Positional : detail::ConfigBase<config::Positional> {
			detail::Positional positional;
			constexpr Positional(std::wstring name, const arger::Type& type, std::wstring description = L"") : positional{ name, type, description } {}
			constexpr void apply(arger::Configuration& config) const {
				config.positional.push_back(positional);
			}
		};

		/* [global/groups] add another help-string to the given group or the global list */
		struct Help : detail::ConfigBase<config::Help> {
			detail::Help help;
			constexpr Help(std::wstring name, std::wstring description) : help{ name, description } {}
			constexpr void apply(arger::Configuration& config) const {
				config.help.push_back(help);
			}
		};
	}
}
