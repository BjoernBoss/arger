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
		struct Endpoints {
			std::vector<arger::Endpoint> endpoints;
		};
		struct EndpointId {
			size_t endpointId = 0;
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

		struct Arguments :
			public detail::Require,
			public detail::Positionals,
			public detail::Constraint,
			public detail::Groups,
			public detail::Endpoints,
			public detail::EndpointId {
		};
		struct Group :
			public detail::Config,
			public detail::Description,
			public detail::Information,
			public detail::Use,
			public detail::Arguments,
			public detail::Abbreviation,
			public detail::Options {
		};

		struct ConfigApply {
			template <class Base>
			static constexpr void Apply(Base& base) {}
			template <class Base, class Config, class... Configs>
			static constexpr void Apply(Base& base, const Config& config, const Configs&... configs) {
				config.apply(base);
				detail::ConfigApply::Apply<Base, Configs...>(base, configs...);
			}
		};
	}

	template <class Type, class Base>
	concept IsConfig = std::is_base_of_v<detail::Config, Type>;

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
		friend struct detail::ConfigApply;
	public:
		std::wstring name;
		size_t id = 0;

	public:
		constexpr Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Option> auto&... configs);

	private:
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
		friend struct detail::ConfigApply;
	public:
		size_t id = 0;

	public:
		constexpr Endpoint(const arger::IsConfig<arger::Endpoint> auto&... configs);
		constexpr Endpoint(arger::IsId auto id, const arger::IsConfig<arger::Endpoint> auto&... configs);

	private:
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
		friend struct detail::ConfigApply;
	public:
		std::wstring name;
		size_t id = 0;

	public:
		constexpr Group(std::wstring name, const arger::IsConfig<arger::Group> auto&... configs);
		constexpr Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Group> auto&... configs);

	private:
		constexpr void apply(detail::Groups& base) const {
			base.groups.list.push_back(*this);
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which
	*	triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct HelpEntry :
		public detail::Config,
		public detail::SpecialEntry {
		friend struct detail::ConfigApply;
	public:
		constexpr HelpEntry(std::wstring name, const arger::IsConfig<arger::HelpEntry> auto&... configs) : SpecialEntry{ name } {
			detail::ConfigApply::Apply(*this, configs...);
		}

	private:
		constexpr void apply(detail::SpecialEntries& base) const {
			base.special.help = *this;
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which
	*	triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
	struct VersionEntry :
		public detail::Config,
		public detail::SpecialEntry {
		friend struct detail::ConfigApply;
	public:
		constexpr VersionEntry(std::wstring name, const arger::IsConfig<arger::VersionEntry> auto&... configs) : SpecialEntry{ name } {
			detail::ConfigApply::Apply(*this, configs...);
		}

	private:
		constexpr void apply(detail::SpecialEntries& base) const {
			base.special.version = *this;
		}
	};

	/* version text for the current configuration (preceeded by program name, if not in menu-mode) */
	struct VersionText : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::wstring pText;

	public:
		constexpr VersionText(std::wstring text) : pText{ text } {}

	private:
		constexpr void apply(detail::VersionText& base) const {
			base.version = pText;
		}
	};

	/* default alternative program name for the configuration (no program implies menu mode) */
	struct Program : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::wstring pProgram;

	public:
		constexpr Program(std::wstring program) : pProgram{ program } {}

	private:
		constexpr void apply(detail::Program& base) const {
			base.program = pProgram;
		}
	};

	/* description to the corresponding object (all children configures if the text should be printed for all subsequent children
	*	as well; only applies to optional descriptions, such as group descriptions of parents or the config description) */
	struct Description : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::wstring pDescription;
		bool pAllChildren = false;

	public:
		constexpr Description(std::wstring desc, bool allChildren = true) : pDescription{ desc }, pAllChildren{ allChildren } {}

	private:
		constexpr void apply(detail::Description& base) const {
			base.description.text = pDescription;
			base.description.allChildren = pAllChildren;
		}
	};

	/* add information-string to the corresponding object (all children configures if the text should be printed for all subsequent children as well) */
	struct Information : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		detail::Information::Entry entry;

	public:
		constexpr Information(std::wstring name, std::wstring text, bool allChildren = true) : entry{ name, text, allChildren } {}
		constexpr void apply(detail::Information& base) const {
			base.information.push_back(entry);
		}
	};

	/* add a constraint to be executed if the corresponding object is selected via the arguments */
	struct Constraint : public detail::Config {
		friend struct detail::ConfigApply;
	private:
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
		friend struct detail::ConfigApply;
	private:
		size_t pMinimum = 0;
		std::optional<size_t> pMaximum;

	public:
		constexpr Require(size_t min = 1) : pMinimum{ min } {}
		constexpr Require(size_t min, size_t max) : pMinimum{ min }, pMaximum{ max } {}

	private:
		constexpr void apply(detail::Require& base) const {
			base.require.minimum = pMinimum;
			base.require.maximum = pMaximum;
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
		friend struct detail::ConfigApply;
	private:
		wchar_t pChar = 0;

	public:
		constexpr Abbreviation(wchar_t c) : pChar{ c } {}

	private:
		constexpr void apply(detail::Abbreviation& base) const {
			base.abbreviation = pChar;
		}
	};

	/* set the endpoint id of the implicitly defined endpoint
	*	Note: cannot be used in conjunction with explicitly defined endpoints */
	struct EndpointId : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		size_t pId = 0;

	public:
		constexpr EndpointId(arger::IsId auto id) : pId{ static_cast<size_t>(id) } {}

	private:
		constexpr void apply(detail::EndpointId& base) const {
			base.endpointId = pId;
		}
	};

	/* add a payload to an option with a given name and of a given type, and optional default values (must
	*	meet the requirement-counts), will be used to fill up parsed values, if less were provided */
	struct Payload : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::vector<arger::Value> pDefValue;
		std::wstring pName;
		arger::Type pType;

	public:
		Payload(std::wstring name, arger::Type type, std::vector<arger::Value> defValue = {}) : pDefValue{ defValue }, pName{ name }, pType{ type } {}
		Payload(std::wstring name, arger::Type type, arger::Value defValue) : pDefValue{ defValue }, pName{ name }, pType{ type } {}

	private:
		constexpr void apply(detail::Payload& base) const {
			base.payload.defValue = pDefValue;
			base.payload.name = pName;
			base.payload.type = pType;
		}
	};

	/* add usage-constraints to let the corresponding options only be used by groups,
	*	which add them as usage (by default every group/argument can use all options) */
	struct Use : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::set<size_t> pOptions;

	public:
		Use(arger::IsId auto... options) : pOptions{ static_cast<size_t>(options)... } {}

	private:
		void apply(detail::Use& base) const {
			base.use.insert(pOptions.begin(), pOptions.end());
		}
	};

	/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
	struct GroupName : detail::Config {
		friend struct detail::ConfigApply;
	private:
		std::wstring pName;

	public:
		constexpr GroupName(std::wstring name) : pName{ name } {}

	private:
		constexpr void apply(detail::Groups& base) const {
			base.groups.name = pName;
		}
	};

	/* add an additional positional argument to the configuration/group using the given name, type, description, and optional default value
	*	Note: Must meet the requirement-counts
	*	Note: Groups/Configs can can only have sub-groups or positional arguments
	*	Note: Default values will be used, when no argument is given */
	struct Positional : public detail::Config {
		friend struct detail::ConfigApply;
	private:
		detail::Positionals::Entry pEntry;

	public:
		Positional(std::wstring name, arger::Type type, std::wstring description) : pEntry{ std::nullopt, name, type, description } {}
		Positional(std::wstring name, arger::Type type, std::wstring description, arger::Value defValue) : pEntry{ defValue, name, type, description } {}

	private:
		constexpr void apply(detail::Positionals& base) const {
			base.positionals.push_back(pEntry);
		}
	};

	constexpr arger::Config::Config(const arger::IsConfig<arger::Config> auto&... configs) {
		detail::ConfigApply::Apply(*this, configs...);
	}

	constexpr arger::Option::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Option> auto&... configs) : name{ name }, id{ static_cast<size_t>(id) } {
		detail::ConfigApply::Apply(*this, configs...);
	}

	constexpr arger::Endpoint::Endpoint(const arger::IsConfig<arger::Endpoint> auto&... configs) : id{ 0 } {
		detail::ConfigApply::Apply(*this, configs...);
	}
	constexpr arger::Endpoint::Endpoint(arger::IsId auto id, const arger::IsConfig<arger::Endpoint> auto&... configs) : id{ static_cast<size_t>(id) } {
		detail::ConfigApply::Apply(*this, configs...);
	}

	constexpr arger::Group::Group(std::wstring name, const arger::IsConfig<arger::Group> auto&... configs) : name{ name }, id{ 0 } {
		detail::ConfigApply::Apply(*this, configs...);
	}
	constexpr arger::Group::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Group> auto&... configs) : name{ name }, id{ static_cast<size_t>(id) } {
		detail::ConfigApply::Apply(*this, configs...);
	}
}
