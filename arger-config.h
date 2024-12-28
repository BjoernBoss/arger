/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"

namespace arger {
	struct Group;
	struct Option;

	namespace detail {
		struct Config {};

		struct Description {
			std::wstring description;
		};
		struct Help {
		public:
			struct Entry {
				std::wstring name;
				std::wstring text;
			};

		public:
			std::vector<Entry> help;
		};
		struct Constraint {
			std::vector<arger::Checker> constraints;
		};
		struct Require {
			struct {
				std::optional<size_t> minimum;
				std::optional<size_t> maximum;
			} require;
		};
		struct Abbreviation {
			wchar_t abbreviation = 0;
		};
		struct Payload {
			struct {
				std::wstring name;
				arger::Type type;
			} payload;
		};
		struct Use {
			std::set<std::wstring> use;
		};
		struct SpecialPurpose {
			struct {
				bool help = false;
				bool version = false;
			} specialPurpose;
		};
		struct Positionals {
		public:
			struct Entry {
				std::wstring name;
				arger::Type type;
				std::wstring description;
			};

		public:
			std::vector<Entry> positionals;
		};
		struct Options {
			std::vector<arger::Option> options;
		};
		struct Groups {
			struct {
				std::vector<arger::Group> list;
				std::wstring name;
			} groups;
		};

		template <class Base, class Config>
		constexpr void ApplyConfigs(Base& base, const Config& config) {
			config.apply(base);
		}
		template <class Base, class Config, class... Configs>
		constexpr void ApplyConfigs(Base& base, const Config& config, const Configs&... configs) {
			config.apply(base);
			ApplyConfigs<Base, Configs...>(base, configs...);
		}

		struct Arguments :
			public detail::Require,
			public detail::Positionals,
			public detail::Constraint,
			public detail::Groups {
		};
	}

	template <class Type, class Base>
	concept IsConfig = std::is_base_of_v<detail::Config, Type>&& requires(const Type t, Base b) {
		t.apply(b);
	};

	/* general arger-configuration to be parsed */
	struct Config :
		public detail::Description,
		public detail::Help,
		public detail::Options,
		public detail::Arguments {
	public:
		std::wstring program;
		std::wstring version;

	public:
		constexpr Config(std::wstring program, std::wstring version);
		constexpr Config(std::wstring program, std::wstring version, const arger::IsConfig<arger::Config> auto&... configs);
	};

	/* general sub-group of options for a configuration/group
	*	 Note: can only have sub-groups or positional arguments */
	struct Group :
		public detail::Config,
		public detail::Description,
		public detail::Help,
		public detail::Use,
		public detail::Arguments {
	public:
		std::wstring name;
		std::wstring id;

	public:
		Group(std::wstring name, std::wstring id);
		constexpr Group(std::wstring name, std::wstring id, const arger::IsConfig<arger::Group> auto&... configs);
		constexpr void apply(detail::Groups& base) const {
			base.groups.list.push_back(*this);
		}
	};

	/* general optional flag/payload */
	struct Option :
		public detail::Config,
		public detail::Description,
		public detail::Constraint,
		public detail::Require,
		public detail::Abbreviation,
		public detail::Payload,
		public detail::SpecialPurpose {
	public:
		std::wstring name;

	public:
		Option(std::wstring name);
		constexpr Option(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs);
		constexpr void apply(detail::Options& base) const {
			base.options.push_back(*this);
		}
	};

	constexpr arger::Config::Config(std::wstring program, std::wstring version) : program{ program }, version{ version } {}
	constexpr arger::Config::Config(std::wstring program, std::wstring version, const arger::IsConfig<arger::Config> auto&... configs) : program{ program }, version{ version } {
		detail::ApplyConfigs(*this, configs...);
	}

	arger::Group::Group(std::wstring name, std::wstring id) : name{ name }, id{ id } {}
	constexpr arger::Group::Group(std::wstring name, std::wstring id, const arger::IsConfig<arger::Group> auto&... configs) : name{ name }, id{ id } {
		detail::ApplyConfigs(*this, configs...);
	}

	arger::Option::Option(std::wstring name) : name{ name } {}
	constexpr arger::Option::Option(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs) : name{ name } {
		detail::ApplyConfigs(*this, configs...);
	}

	/* description to the corresponding object */
	struct Description : public detail::Config {
	public:
		std::wstring desc;

	public:
		constexpr Description(std::wstring desc) : desc{ desc } {}
		constexpr void apply(detail::Description& base) const {
			base.description = desc;
		}
	};

	/* add help-string to the corresponding object */
	struct Help : public detail::Config {
	public:
		detail::Help::Entry entry;

	public:
		constexpr Help(std::wstring name, std::wstring text) : entry{ name, text } {}
		constexpr void apply(detail::Help& base) const {
			base.help.push_back(entry);
		}
	};

	/* add a constraint to be executed if the corresponding object is selected via the arguments */
	struct Constraint : public detail::Config {
	public:
		arger::Checker constraint;

	public:
		Constraint(arger::Checker constraint) : constraint{ constraint } {}
		constexpr void apply(detail::Constraint& base) const {
			base.constraints.push_back(constraint);
		}
	};

	/* add a minimum/maximum requirement [maximum=0 implies no maximum]
	*	- [Option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
	*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
	*		if greater than number of positional arguments, last type is used as catch-all */
	struct Require : public detail::Config {
	public:
		size_t minimum = 0;
		std::optional<size_t> maximum;

	public:
		constexpr Require(size_t min = 1) : minimum{ min } {}
		constexpr Require(size_t min, size_t max) : minimum{ min }, maximum{ max } {}
		constexpr void apply(detail::Require& base) const {
			base.require.minimum = minimum;
			base.require.maximum = maximum;
		}
	};

	/* add an abbreviation character for an option to allow it to be accessible as, for example, -x */
	struct Abbreviation : public detail::Config {
	public:
		wchar_t chr = 0;

	public:
		constexpr Abbreviation(wchar_t c) : chr{ c } {}
		constexpr void apply(detail::Abbreviation& base) const {
			base.abbreviation = chr;
		}
	};

	/* add a payload to an option with a given name and of a given type */
	struct Payload : public detail::Config {
	public:
		std::wstring name;
		arger::Type type;

	public:
		Payload(std::wstring name, arger::Type type) : name{ name }, type{ type } {}
		constexpr void apply(detail::Payload& base) const {
			base.payload.name = name;
			base.payload.type = type;
		}
	};

	/* add usage-constraints to let the corresponding options only be used by groups, which add them as usage (by default every group/argument can use all options) */
	struct Use : public detail::Config {
	public:
		std::set<std::wstring> options;

	public:
		Use(const auto&... options) : options{ options... } {}
		void apply(detail::Use& base) const {
			base.use.insert(options.begin(), options.end());
		}
	};

	/* mark this flag as being the help-indicating flag, which triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct HelpFlag : detail::Config {
	public:
		constexpr HelpFlag() {}
		constexpr void apply(detail::SpecialPurpose& base) const {
			base.specialPurpose.help = true;
		}
	};

	/* mark this flag as being the version-indicating flag, which triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct VersionFlag : detail::Config {
	public:
		constexpr VersionFlag() {}
		constexpr void apply(detail::SpecialPurpose& base) const {
			base.specialPurpose.version = true;
		}
	};

	/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
	struct GroupName : detail::Config {
	public:
		std::wstring name;

	public:
		constexpr GroupName(std::wstring name) : name{ name } {}
		constexpr void apply(detail::Groups& base) const {
			base.groups.name = name;
		}
	};

	/* add an additional positional argument to the configuration/group using the given name, type, and description
	*	 Note: can only have sub-groups or positional arguments */
	struct Positional : public detail::Config {
	public:
		detail::Positionals::Entry entry;

	public:
		Positional(std::wstring name, arger::Type type, std::wstring description) : entry{ name, type, description } {}
		constexpr void apply(detail::Positionals& base) const {
			base.positionals.push_back(entry);
		}
	};
}
