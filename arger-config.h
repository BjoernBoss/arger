/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-value.h"

namespace arger {
	struct Group;
	struct Option;
	struct Endpoint;

	namespace detail {
		struct Config {};

		struct VersionText {
			std::wstring version;
		};
		struct Program {
			std::wstring program;
		};
		struct Description {
			struct {
				std::wstring text;
				bool allChildren = false;
			} description;
		};
		struct Information {
		public:
			struct Entry {
				std::wstring name;
				std::wstring text;
				bool allChildren = false;
			};

		public:
			std::vector<Entry> information;
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
				std::vector<arger::Value> defValue;
				std::wstring name;
				arger::Type type;
			} payload;
		};
		struct Use {
			std::set<size_t> use;
		};
		struct Positionals {
		public:
			struct Entry {
				std::optional<arger::Value> defValue;
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
		struct SpecialEntry :
			public detail::Description,
			public detail::Abbreviation {
			std::wstring name;
			constexpr SpecialEntry() {}
			constexpr SpecialEntry(std::wstring name) : name{ name } {}
		};
		struct SpecialEntries {
			struct {
				detail::SpecialEntry help;
				detail::SpecialEntry version;
			} special;
		};
		struct Endpoints {
			std::vector<arger::Endpoint> endpoints;
		};
		struct EndpointId {
			size_t endpointId = 0;
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
			public detail::Groups,
			public detail::Endpoints,
			public detail::EndpointId {
		};
	}

	template <class Type, class Base>
	concept IsConfig = std::is_base_of_v<detail::Config, Type>&& requires(const Type t, Base b) {
		t.apply(b);
	};

	/* general arger-configuration to be parsed */
	struct Config :
		public detail::Description,
		public detail::Information,
		public detail::Options,
		public detail::Arguments,
		public detail::VersionText,
		public detail::Program,
		public detail::SpecialEntries {
	public:
		constexpr Config();
		constexpr Config(const arger::IsConfig<arger::Config> auto&... configs);
	};

	/* general optional flag/payload
	*	Note: if passed to a group, it is implicitly only bound to that group - but all names and abbreviations must be unique */
	struct Option :
		public detail::Config,
		public detail::Description,
		public detail::Constraint,
		public detail::Require,
		public detail::Abbreviation,
		public detail::Payload {
	public:
		std::wstring name;
		size_t id = 0;

	public:
		Option(std::wstring name, arger::IsId auto id);
		constexpr Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Option> auto&... configs);
		constexpr void apply(detail::Options& base) const {
			base.options.push_back(*this);
		}
	};

	/* endpoint for positional arguments for a configuration/group (to enable a group to have multiple variations of positional counts)
	*	 Note: If Groups/Configs define positional arguments directly, an implicit endpoint is defined and no further endpoints can be added */
	struct Endpoint :
		public detail::Config,
		public detail::Require,
		public detail::Positionals,
		public detail::Constraint,
		public detail::Description {
	public:
		size_t id = 0;

	public:
		Endpoint();
		Endpoint(arger::IsId auto id);
		constexpr Endpoint(const arger::IsConfig<arger::Endpoint> auto&... configs);
		constexpr Endpoint(arger::IsId auto id, const arger::IsConfig<arger::Endpoint> auto&... configs);
		constexpr void apply(detail::Endpoints& base) const {
			base.endpoints.push_back(*this);
		}
	};

	/* general sub-group of options for a configuration/group
	*	 Note: Groups/Configs can can only have sub-groups or positional arguments */
	struct Group :
		public detail::Config,
		public detail::Description,
		public detail::Information,
		public detail::Use,
		public detail::Arguments,
		public detail::Abbreviation,
		public detail::Options {
	public:
		std::wstring name;
		size_t id = 0;

	public:
		Group(std::wstring name);
		Group(std::wstring name, arger::IsId auto id);
		constexpr Group(std::wstring name, const arger::IsConfig<arger::Group> auto&... configs);
		constexpr Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Group> auto&... configs);
		constexpr void apply(detail::Groups& base) const {
			base.groups.list.push_back(*this);
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct HelpEntry :
		public detail::Config,
		public detail::SpecialEntry {
	public:
		constexpr HelpEntry(std::wstring name) : SpecialEntry{ name } {}
		constexpr HelpEntry(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs) : SpecialEntry{ name } {
			detail::ApplyConfigs(*this, configs...);
		}
		constexpr void apply(detail::SpecialEntries& base) const {
			base.special.help = *this;
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct VersionEntry :
		public detail::Config,
		public detail::SpecialEntry {
	public:
		constexpr VersionEntry(std::wstring name) : SpecialEntry{ name } {}
		constexpr VersionEntry(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs) : SpecialEntry{ name } {
			detail::ApplyConfigs(*this, configs...);
		}
		constexpr void apply(detail::SpecialEntries& base) const {
			base.special.version = *this;
		}
	};

	constexpr arger::Config::Config() {}
	constexpr arger::Config::Config(const arger::IsConfig<arger::Config> auto&... configs) {
		detail::ApplyConfigs(*this, configs...);
	}

	inline arger::Option::Option(std::wstring name, arger::IsId auto id) : name{ name }, id{ static_cast<size_t>(id) } {}
	constexpr arger::Option::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Option> auto&... configs) : name{ name }, id{ static_cast<size_t>(id) } {
		detail::ApplyConfigs(*this, configs...);
	}

	inline arger::Endpoint::Endpoint() : id{ 0 } {}
	inline arger::Endpoint::Endpoint(arger::IsId auto id) : id{ static_cast<size_t>(id) } {}
	constexpr arger::Endpoint::Endpoint(const arger::IsConfig<arger::Endpoint> auto&... configs) : id{ 0 } {
		detail::ApplyConfigs(*this, configs...);
	}
	constexpr arger::Endpoint::Endpoint(arger::IsId auto id, const arger::IsConfig<arger::Endpoint> auto&... configs) : id{ static_cast<size_t>(id) } {
		detail::ApplyConfigs(*this, configs...);
	}

	inline arger::Group::Group(std::wstring name) : name{ name }, id{ 0 } {}
	inline arger::Group::Group(std::wstring name, arger::IsId auto id) : name{ name }, id{ static_cast<size_t>(id) } {}
	constexpr arger::Group::Group(std::wstring name, const arger::IsConfig<arger::Group> auto&... configs) : name{ name }, id{ 0 } {
		detail::ApplyConfigs(*this, configs...);
	}
	constexpr arger::Group::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Group> auto&... configs) : name{ name }, id{ static_cast<size_t>(id) } {
		detail::ApplyConfigs(*this, configs...);
	}

	/* version text for the current configuration (preceeded by program name, if not in menu-mode) */
	struct VersionText : public detail::Config {
	public:
		std::wstring text;

	public:
		constexpr VersionText(std::wstring text) : text{ text } {}
		constexpr void apply(detail::VersionText& base) const {
			base.version = text;
		}
	};

	/* default alternative program name for the configuration (no program implies menu mode) */
	struct Program : public detail::Config {
	public:
		std::wstring program;

	public:
		constexpr Program(std::wstring program) : program{ program } {}
		constexpr void apply(detail::Program& base) const {
			base.program = program;
		}
	};

	/* description to the corresponding object (all children only applies to optional descriptions) */
	struct Description : public detail::Config {
	public:
		std::wstring desc;
		bool allChildren = false;

	public:
		constexpr Description(std::wstring desc, bool allChildren = true) : desc{ desc }, allChildren{ allChildren } {}
		constexpr void apply(detail::Description& base) const {
			base.description.text = desc;
			base.description.allChildren = allChildren;
		}
	};

	/* add information-string to the corresponding object (all children configures if the text should be printed for all subsequent children as well) */
	struct Information : public detail::Config {
	public:
		detail::Information::Entry entry;

	public:
		constexpr Information(std::wstring name, std::wstring text, bool allChildren = true) : entry{ name, text, allChildren } {}
		constexpr void apply(detail::Information& base) const {
			base.information.push_back(entry);
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

	public:
		static arger::Require AtLeast(size_t min) {
			return{ min, 0 };
		}
		static arger::Require Any() {
			return{ 0, 0 };
		}
	};

	/* add an abbreviation character for an option, group, or help/version entry to allow it to be accessible as single letters or, for example, -x */
	struct Abbreviation : public detail::Config {
	public:
		wchar_t chr = 0;

	public:
		constexpr Abbreviation(wchar_t c) : chr{ c } {}
		constexpr void apply(detail::Abbreviation& base) const {
			base.abbreviation = chr;
		}
	};

	/* set the endpoint id of the implicitly defined endpoint
	*	Note: cannot be used in conjunction with explicitly defined endpoints */
	struct EndpointId : public detail::Config {
	public:
		size_t id = 0;

	public:
		constexpr EndpointId(arger::IsId auto id) : id{ static_cast<size_t>(id) } {}
		constexpr void apply(detail::EndpointId& base) const {
			base.endpointId = id;
		}
	};

	/* add a payload to an option with a given name and of a given type, and optional default values (must meet the requirement-counts) */
	struct Payload : public detail::Config {
	public:
		std::vector<arger::Value> defValue;
		std::wstring name;
		arger::Type type;

	public:
		Payload(std::wstring name, arger::Type type, std::vector<arger::Value> defValue = {}) : defValue{ defValue }, name{ name }, type{ type } {}
		Payload(std::wstring name, arger::Type type, arger::Value defValue) : defValue{ defValue }, name{ name }, type{ type } {}
		constexpr void apply(detail::Payload& base) const {
			base.payload.defValue = defValue;
			base.payload.name = name;
			base.payload.type = type;
		}
	};

	/* add usage-constraints to let the corresponding options only be used by groups, which add them as usage (by default every group/argument can use all options) */
	struct Use : public detail::Config {
	public:
		std::set<size_t> options;

	public:
		Use(arger::IsId auto... options) : options{ static_cast<size_t>(options)... } {}
		void apply(detail::Use& base) const {
			base.use.insert(options.begin(), options.end());
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

	/* add an additional positional argument to the configuration/group using the given name, type, description, and optional default value (must meet the requirement-counts)
	*	Note: Groups/Configs can can only have sub-groups or positional arguments
	*	Note: Default values will be used, when no argument is given, or the argument string is empty */
	struct Positional : public detail::Config {
	public:
		detail::Positionals::Entry entry;

	public:
		Positional(std::wstring name, arger::Type type, std::wstring description) : entry{ std::nullopt, name, type, description } {}
		Positional(std::wstring name, arger::Type type, std::wstring description, arger::Value defValue) : entry{ defValue, name, type, description } {}
		constexpr void apply(detail::Positionals& base) const {
			base.positionals.push_back(entry);
		}
	};
}
