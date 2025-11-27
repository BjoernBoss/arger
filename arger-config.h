/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-value.h"

namespace arger {
	struct Config;

	namespace detail {
		struct Configurator {};

		struct VersionText {
			std::wstring version;
		};
		struct Program {
			std::wstring program;
		};
		struct Description {
			struct {
				std::wstring normal;
				std::wstring reduced;
			} description;
		};
		struct Information {
		public:
			struct Entry {
				std::wstring name;
				std::wstring text;
				bool normal = false;
				bool reduced = false;
				bool allChildren = false;
			};

		public:
			std::vector<Entry> information;
		};
		struct Hidden {
			bool hidden = false;
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
		struct Positional :
			public detail::Description {
			std::optional<arger::Value> defValue;
			std::wstring name;
			arger::Type type;
			Positional(std::optional<arger::Value> val, std::wstring name, arger::Type type) : defValue{ val }, name{ name }, type{ type } {}
		};
		struct Positionals {
			std::vector<detail::Positional> positionals;
		};
		struct EndpointId {
			size_t endpointId = 0;
		};

		struct Endpoint :
			public detail::Require,
			public detail::Positionals,
			public detail::Constraint,
			public detail::Description,
			public detail::Hidden {
			size_t id = 0;
			Endpoint(size_t id) : id{ id } {}
		};
		struct Endpoints {
			std::vector<detail::Endpoint> endpoints;
		};

		struct Option :
			public detail::Description,
			public detail::Constraint,
			public detail::Require,
			public detail::Abbreviation,
			public detail::Payload,
			public detail::Hidden {
			std::wstring name;
			size_t id = 0;
			Option(std::wstring name, size_t id) : name{ name }, id{ id } {}
		};
		struct Options {
			std::vector<detail::Option> options;
		};

		struct Group;
		struct Groups {
			struct {
				std::vector<detail::Group> list;
				std::wstring name;
			} groups;
		};

		struct SpecialEntry :
			public detail::Description,
			public detail::Abbreviation {
			std::wstring name;
			bool allChildren = false;
			bool reducible = false;
			constexpr SpecialEntry() {}
			constexpr SpecialEntry(std::wstring name, bool allChildren, bool reducible) : name{ name }, allChildren{ allChildren }, reducible{ reducible } {}
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
			public detail::Options,
			public detail::Description,
			public detail::Information,
			public detail::Endpoints,
			public detail::EndpointId {
		};
		struct Group :
			public detail::Use,
			public detail::Arguments,
			public detail::Abbreviation,
			public detail::Hidden {
			std::wstring name;
			size_t id = 0;
			Group(std::wstring name, size_t id) : name{ name }, id{ id } {}
		};

		struct Config :
			public detail::Program,
			public detail::Arguments,
			public detail::VersionText,
			public detail::SpecialEntries {
		};

		struct ConfigBurner {
			template <class Base>
			static constexpr void Apply(Base& base) {}
			template <class Base, class Config, class... Configs>
			static constexpr void Apply(Base& base, const Config& config, const Configs&... configs) {
				config.burnConfig(base);
				detail::ConfigBurner::Apply<Base, Configs...>(base, configs...);
			}

			template <class Base, class Config>
			static decltype(std::declval<Config>().burnConfig(std::declval<Base&>()), std::true_type{}) CanBurn(int) { return {}; }
			template <class, class>
			static std::false_type CanBurn(...) { return {}; }

			static const detail::Config& GetBurned(const arger::Config& config);
		};
	}

	template <class Type, class Base>
	concept IsConfig = std::is_base_of_v<detail::Configurator, Type>&& decltype(detail::ConfigBurner::CanBurn<Base, Type>(0))::value;

	/* general arger-configuration to be parsed */
	struct Config {
		friend struct detail::ConfigBurner;
	private:
		detail::Config pConfig;

	public:
		constexpr Config(const arger::IsConfig<detail::Config> auto&... configs);
		constexpr arger::Config& add(const arger::IsConfig<detail::Config> auto&... configs);
	};

	/* general optional flag/payload
	*	Note: if passed to a group, it is implicitly only bound to that group - but all names and abbreviations must be unique */
	struct Option : public detail::Configurator {
		friend struct detail::ConfigBurner;
	public:
		detail::Option pOption;

	public:
		constexpr Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Option> auto&... configs);
		constexpr arger::Option& add(const arger::IsConfig<detail::Option> auto&... configs);

	private:
		constexpr void burnConfig(detail::Options& base) const {
			base.options.push_back(pOption);
		}
	};

	/* endpoint for positional arguments for a configuration/group (to enable a group to have multiple variations of positional counts)
	*	 Note: If Groups/Configs define positional arguments directly, an implicit endpoint is defined and no further endpoints can be added */
	struct Endpoint : public detail::Configurator {
		friend struct detail::ConfigBurner;
	public:
		detail::Endpoint pEndpoint;

	public:
		constexpr Endpoint(const arger::IsConfig<detail::Endpoint> auto&... configs);
		constexpr Endpoint(arger::IsId auto id, const arger::IsConfig<detail::Endpoint> auto&... configs);
		constexpr arger::Endpoint& add(const arger::IsConfig<detail::Endpoint> auto&... configs);

	private:
		constexpr void burnConfig(detail::Endpoints& base) const {
			base.endpoints.push_back(pEndpoint);
		}
	};

	/* general sub-group of options for a configuration/group
	*	 Note: Groups/Configs can only either have sub-groups or positional arguments */
	struct Group : public detail::Configurator {
		friend struct detail::ConfigBurner;
	public:
		detail::Group pGroup;

	public:
		constexpr Group(std::wstring name, const arger::IsConfig<detail::Group> auto&... configs);
		constexpr Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Group> auto&... configs);
		constexpr arger::Group& add(const arger::IsConfig<detail::Group> auto&... configs);

	private:
		constexpr void burnConfig(detail::Groups& base) const {
			base.groups.list.push_back(pGroup);
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which
	*	triggers the help-menu to be printed (prior to verifying the remainder of the argument structure)
	*	Note: if reducible is enabled, the usage of the abbreviation will only print the reduced help menu
	*	Note: if all-children is enabled, the help entry will be printed as option in all sub-groups as well */
	struct HelpEntry : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		detail::SpecialEntry pSpecial;

	public:
		constexpr HelpEntry(std::wstring name, bool allChildren, bool reducible, const arger::IsConfig<detail::SpecialEntry> auto&... configs) : pSpecial{ name, allChildren, reducible } {
			detail::ConfigBurner::Apply(pSpecial, configs...);
		}
		constexpr arger::HelpEntry& add(const arger::IsConfig<detail::SpecialEntry> auto&... configs) {
			detail::ConfigBurner::Apply(pSpecial, configs...);
			return *this;
		}

	private:
		constexpr void burnConfig(detail::SpecialEntries& base) const {
			base.special.help = pSpecial;
		}
	};

	/* configure the key to be used as option for argument mode and any group name for menu mode, which
	*	triggers the version-menu to be printed (prior to verifying the remainder of the argument structure)
	*	Note: if all-children is enabled, the help entry will be printed as option in all sub-groups as well */
	struct VersionEntry : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		detail::SpecialEntry pSpecial;

	public:
		constexpr VersionEntry(std::wstring name, bool allChildren, const arger::IsConfig<detail::SpecialEntry> auto&... configs) : pSpecial{ name, allChildren, false } {
			detail::ConfigBurner::Apply(pSpecial, configs...);
		}
		constexpr arger::VersionEntry& add(const arger::IsConfig<detail::SpecialEntry> auto&... configs) {
			detail::ConfigBurner::Apply(pSpecial, configs...);
			return *this;
		}

	private:
		constexpr void burnConfig(detail::SpecialEntries& base) const {
			base.special.version = pSpecial;
		}
	};

	/* version text for the current configuration (preceeded by program name, if not in menu-mode) */
	struct VersionText : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::wstring pText;

	public:
		constexpr VersionText(std::wstring text) : pText{ text } {}

	private:
		constexpr void burnConfig(detail::VersionText& base) const {
			base.version = pText;
		}
	};

	/* default alternative program name for the configuration (no program implies menu mode) */
	struct Program : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::wstring pProgram;

	public:
		constexpr Program(std::wstring program) : pProgram{ program } {}

	private:
		constexpr void burnConfig(detail::Program& base) const {
			base.program = pProgram;
		}
	};

	/* description to the corresponding object
	*	Note: if reduced text is used, it will be used for reduced help menus */
	struct Description : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::wstring pDescription;
		std::wstring pReduced;

	public:
		constexpr Description(std::wstring desc) : pDescription{ desc } {}
		constexpr Description(std::wstring reduced, std::wstring desc) : pReduced{ reduced }, pDescription{ desc } {}

	private:
		constexpr void burnConfig(detail::Description& base) const {
			base.description.normal = pDescription;
			base.description.reduced = pReduced;
		}
	};

	/* add information-string to the corresponding config/group and show them for entry and optionally all children
	*	Note: will print the information optionally in the normal, reduced, or both menus */
	struct Information : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		detail::Information::Entry entry;

	public:
		constexpr Information(std::wstring name, bool normal, bool reduced, bool allChildren, std::wstring text) : entry{ name, text, normal, reduced, allChildren } {}

	private:
		constexpr void burnConfig(detail::Information& base) const {
			base.information.push_back(entry);
		}
	};

	/* add a constraint to be executed if the corresponding object is selected via the arguments */
	struct Constraint : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		arger::Checker constraint;

	public:
		Constraint(arger::Checker constraint) : constraint{ constraint } {}

	private:
		constexpr void burnConfig(detail::Constraint& base) const {
			base.constraints.push_back(constraint);
		}
	};

	/* add a minimum/maximum requirement [maximum < minimum implies no maximum]
	*	if only minimum is supplied, default maximum will be used
	*	- [Option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
	*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
	*		if greater than number of positional arguments, last type is used as catch-all */
	struct Require : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		size_t pMinimum = 0;
		std::optional<size_t> pMaximum;

	public:
		constexpr Require(size_t min = 1) : pMinimum{ min } {}
		constexpr Require(size_t min, size_t max) : pMinimum{ min }, pMaximum{ max } {}
		static arger::Require AtLeast(size_t min) {
			return{ min, 0 };
		}
		static arger::Require Any() {
			return{ 0, 0 };
		}

	private:
		constexpr void burnConfig(detail::Require& base) const {
			base.require.minimum = pMinimum;
			base.require.maximum = pMaximum;
		}
	};

	/* add an abbreviation character for an option, group, or help/version entry to allow it to be accessible as single letters or, for example, -x */
	struct Abbreviation : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		wchar_t pChar = 0;

	public:
		constexpr Abbreviation(wchar_t c) : pChar{ c } {}

	private:
		constexpr void burnConfig(detail::Abbreviation& base) const {
			base.abbreviation = pChar;
		}
	};

	/* set the endpoint id of the implicitly defined endpoint
	*	Note: cannot be used in conjunction with explicitly defined endpoints */
	struct EndpointId : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		size_t pId = 0;

	public:
		constexpr EndpointId(arger::IsId auto id) : pId{ static_cast<size_t>(id) } {}

	private:
		constexpr void burnConfig(detail::EndpointId& base) const {
			base.endpointId = pId;
		}
	};

	/* add a payload to an option with a given name and of a given type */
	struct Payload : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::wstring pName;
		arger::Type pType;

	public:
		Payload(std::wstring name, arger::Type type) : pName{ name }, pType{ type } {}

	private:
		constexpr void burnConfig(detail::Payload& base) const {
			base.payload.name = pName;
			base.payload.type = pType;
		}
	};

	/* add usage-constraints to let the corresponding options only be used by groups,
	*	which add them as usage (by default every group/argument can use all options) */
	struct Use : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::set<size_t> pOptions;

	public:
		Use(arger::IsId auto... options) : pOptions{ static_cast<size_t>(options)... } {}

	private:
		void burnConfig(detail::Use& base) const {
			base.use.insert(pOptions.begin(), pOptions.end());
		}
	};

	/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
	struct GroupName : detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		std::wstring pName;

	public:
		constexpr GroupName(std::wstring name) : pName{ name } {}

	private:
		constexpr void burnConfig(detail::Groups& base) const {
			base.groups.name = pName;
		}
	};

	/* add an additional positional argument to the configuration/group using the given name, type, description
	*	Note: Must meet the requirement-counts
	*	Note: Groups/Configs can can only have sub-groups or positional arguments */
	struct Positional : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		detail::Positional pPositional;

	public:
		constexpr Positional(std::wstring name, arger::Type type, const arger::IsConfig<detail::Positional> auto&... configs) : pPositional{ std::nullopt, name, type } {
			detail::ConfigBurner::Apply(pPositional, configs...);
		}
		constexpr arger::Positional& add(const arger::IsConfig<detail::Positional> auto&... configs) {
			detail::ConfigBurner::Apply(pPositional, configs...);
			return *this;
		}

	private:
		constexpr void burnConfig(detail::Positionals& base) const {
			base.positionals.push_back(pPositional);
		}
	};

	/* add a default value to a positional or one or more default values to optionals - will be used when
	*	no values are povided and to fill up remaining values, if less were provided and must meet requirement
	*	counts for optionals/must be applied to all upcoming positionals for the requirement count */
	struct Default : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		arger::Value pDefValue;

	public:
		Default(arger::Value defValue) : pDefValue{ defValue } {}

	private:
		constexpr void burnConfig(detail::Positional& base) const {
			base.defValue = pDefValue;
		}
		constexpr void burnConfig(detail::Payload& base) const {
			base.payload.defValue.push_back(pDefValue);
		}
	};

	/* set the visibility of options/groups/endpoints and their children in the help menu (does not affect parsing) */
	struct Hidden : public detail::Configurator {
		friend struct detail::ConfigBurner;
	private:
		bool pHidden = false;

	public:
		Hidden(bool hidden = true) : pHidden{ hidden } {}

	private:
		constexpr void burnConfig(detail::Hidden& base) const {
			base.hidden = pHidden;
		}
	};

	inline const detail::Config& detail::ConfigBurner::GetBurned(const arger::Config& config) {
		return config.pConfig;
	}

	constexpr arger::Config::Config(const arger::IsConfig<detail::Config> auto&... configs) {
		detail::ConfigBurner::Apply(pConfig, configs...);
	}
	constexpr arger::Config& arger::Config::add(const arger::IsConfig<detail::Config> auto&... configs) {
		detail::ConfigBurner::Apply(pConfig, configs...);
		return *this;
	}

	constexpr arger::Option::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Option> auto&... configs) : pOption{ name, static_cast<size_t>(id) } {
		detail::ConfigBurner::Apply(pOption, configs...);
	}
	constexpr arger::Option& arger::Option::add(const arger::IsConfig<detail::Option> auto&... configs) {
		detail::ConfigBurner::Apply(pOption, configs...);
		return *this;
	}

	constexpr arger::Endpoint::Endpoint(const arger::IsConfig<detail::Endpoint> auto&... configs) : pEndpoint{ 0 } {
		detail::ConfigBurner::Apply(pEndpoint, configs...);
	}
	constexpr arger::Endpoint::Endpoint(arger::IsId auto id, const arger::IsConfig<detail::Endpoint> auto&... configs) : pEndpoint{ static_cast<size_t>(id) } {
		detail::ConfigBurner::Apply(pEndpoint, configs...);
	}
	constexpr arger::Endpoint& arger::Endpoint::add(const arger::IsConfig<detail::Endpoint> auto&... configs) {
		detail::ConfigBurner::Apply(pEndpoint, configs...);
		return *this;
	}

	constexpr arger::Group::Group(std::wstring name, const arger::IsConfig<detail::Group> auto&... configs) : pGroup{ name, 0 } {
		detail::ConfigBurner::Apply(pGroup, configs...);
	}
	constexpr arger::Group::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Group> auto&... configs) : pGroup{ name, static_cast<size_t>(id) } {
		detail::ConfigBurner::Apply(pGroup, configs...);
	}
	constexpr arger::Group& arger::Group::add(const arger::IsConfig<detail::Group> auto&... configs) {
		detail::ConfigBurner::Apply(pGroup, configs...);
		return *this;
	}
}
