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



	struct _Group;
	struct _Option;

	namespace detail {
		struct _Description {
			std::wstring description;
		};
		struct _Help {
		public:
			struct Entry {
				std::wstring name;
				std::wstring text;
			};

		public:
			std::vector<Entry> help;
		};
		struct _Constraint {
			std::vector<arger::Constraint> constraint;
		};
		struct _Require {
			struct {
				std::optional<size_t> minimum;
				std::optional<size_t> maximum;
			} require;
		};
		struct _Abbreviation {
			wchar_t abbreviation = 0;
		};
		struct _Payload {
			struct {
				std::wstring name;
				arger::Type type;
			} payload;
		};
		struct _Use {
			std::set<std::wstring> use;
		};
		struct _SpecialPurpose {
			struct {
				bool help = false;
				bool version = false;
			} specialPurpose;
		};
		struct _Positionals {
		public:
			struct Entry {
				std::wstring name;
				arger::Type type;
				std::wstring description;
			};

		public:
			std::vector<Entry> positionals;
		};
		struct _Options {
			std::vector<arger::_Option> options;
		};
		struct _Groups {
			struct {
				std::vector<arger::_Group> list;
				std::wstring name;
			} groups;
		};

		struct _Config {};

		template <class Base, class Config>
		constexpr void ApplyConfigs(Base& base, const Config& config) {
			config.apply(base);
		}
		template <class Base, class Config, class... Configs>
		constexpr void ApplyConfigs(Base& base, const Config& config, const Configs&... configs) {
			config.apply(base);
			ApplyConfigs<Base, Configs...>(base, configs...);
		}

		struct _Arguments :
			public detail::_Require,
			public detail::_Positionals,
			public detail::_Constraint,
			public detail::_Groups {
		};
	}

	template <class Type, class Base>
	concept IsConfig = std::is_base_of_v<detail::_Config, Type>&& requires(const Type t, Base b) {
		t.apply(b);
	};

	struct _Config :
		public detail::_Description,
		public detail::_Help,
		public detail::_Options,
		public detail::_Arguments {
	public:
		std::wstring program;
		std::wstring version;

	public:
		constexpr _Config(std::wstring program, std::wstring version) : program{ program }, version{ version } {}
		constexpr _Config(std::wstring program, std::wstring version, const arger::IsConfig<arger::_Config> auto&... configs) : program{ program }, version{ version } {
			detail::ApplyConfigs(*this, configs...);
		}
	};

	struct _Group :
		public detail::_Config,
		public detail::_Description,
		public detail::_Help,
		public detail::_Use,
		public detail::_Arguments {
	public:
		std::wstring name;
		std::wstring id;

	public:
		_Group(std::wstring name, std::wstring id) : name{ name }, id{ id } {}
		constexpr _Group(std::wstring name, std::wstring id, const arger::IsConfig<arger::_Group> auto&... configs) : name{ name }, id{ id } {
			detail::ApplyConfigs(*this, configs...);
		}
		constexpr void apply(detail::_Groups& base) const {
			base.groups.list.push_back(*this);
		}
	};

	struct _Option :
		public detail::_Config,
		public detail::_Description,
		public detail::_Constraint,
		public detail::_Require,
		public detail::_Abbreviation,
		public detail::_Payload,
		public detail::_SpecialPurpose {
	public:
		std::wstring name;

	public:
		_Option(std::wstring name) : name{ name } {}
		constexpr _Option(std::wstring name, const arger::IsConfig<arger::_Option> auto&... configs) : name{ name } {
			detail::ApplyConfigs(*this, configs...);
		}
		constexpr void apply(detail::_Options& base) const {
			base.options.push_back(*this);
		}
	};

	struct _Description : public detail::_Config {
	public:
		std::wstring desc;

	public:
		constexpr _Description(std::wstring desc) : desc{ desc } {}
		constexpr void apply(detail::_Description& base) const {
			base.description = desc;
		}
	};

	struct _Help : public detail::_Config {
	public:
		detail::_Help::Entry entry;

	public:
		constexpr _Help(std::wstring name, std::wstring text) : entry{ name, text } {}
		constexpr void apply(detail::_Help& base) const {
			base.help.push_back(entry);
		}
	};

	struct _Constraint : public detail::_Config {
	public:
		arger::Constraint constraint;

	public:
		_Constraint(arger::Constraint constraint) : constraint{ constraint } {}
		constexpr void apply(detail::_Constraint& base) const {
			base.constraint.push_back(constraint);
		}
	};

	struct _Require : public detail::_Config {
	public:
		size_t minimum = 0;
		std::optional<size_t> maximum;

	public:
		constexpr _Require(size_t min = 1) : minimum{ min } {}
		constexpr _Require(size_t min, size_t max) : minimum{ min }, maximum{ max } {}
		constexpr void apply(detail::_Require& base) const {
			base.require.minimum = minimum;
			base.require.maximum = maximum;
		}
	};

	struct _Abbreviation : public detail::_Config {
	public:
		wchar_t chr = 0;

	public:
		constexpr _Abbreviation(wchar_t c) : chr{ c } {}
		constexpr void apply(detail::_Abbreviation& base) const {
			base.abbreviation = chr;
		}
	};

	struct _Payload : public detail::_Config {
	public:
		std::wstring name;
		arger::Type type;

	public:
		constexpr _Payload(std::wstring name, arger::Type type) : name{ name }, type{ type } {}
		constexpr void apply(detail::_Payload& base) const {
			base.payload.name = name;
			base.payload.type = type;
		}
	};

	struct _Use : public detail::_Config {
	public:
		std::set<std::wstring> options;

	public:
		_Use(const auto&... options) : options{ options... } {}
		void apply(detail::_Use& base) const {
			base.use.insert(options.begin(), options.end());
		}
	};

	struct _HelpFlag : detail::_Config {
	public:
		constexpr _HelpFlag() {}
		constexpr void apply(detail::_SpecialPurpose& base) const {
			base.specialPurpose.help = true;
		}
	};

	struct _VersionFlag : detail::_Config {
	public:
		constexpr _VersionFlag() {}
		constexpr void apply(detail::_SpecialPurpose& base) const {
			base.specialPurpose.version = true;
		}
	};

	struct _GroupName : detail::_Config {
	public:
		std::wstring name;

	public:
		constexpr _GroupName(std::wstring name) : name{ name } {}
		constexpr void apply(detail::_Groups& base) const {
			base.groups.name = name;
		}
	};

	struct _Positional : public detail::_Config {
	public:
		detail::_Positionals::Entry entry;

	public:
		constexpr _Positional(std::wstring name, arger::Type type, std::wstring description) : entry{ name, type, description } {}
		constexpr void apply(detail::_Positionals& base) const {
			base.positionals.push_back(entry);
		}
	};
}
