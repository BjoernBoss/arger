/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"

namespace arger::detail {
	struct ValidState {
	public:
		struct Group;
		struct Arguments {
			const detail::_Arguments* self = 0;
			std::map<std::wstring, Group> sub;
			std::wstring_view groupName;
			size_t minimum = 0;
			size_t maximum = 0;
			bool groups = false;
			bool positionals = false;
		};
		struct Group {
			Arguments args;
			const arger::_Group* self = 0;
			std::wstring_view id;
		};
		struct Option {
			const arger::_Option* self = 0;
			std::set<std::wstring> groups;
			size_t minimum = 0;
			size_t maximum = 0;
			bool payload = false;
		};

	public:
		std::map<std::wstring, Option> options;
		std::map<wchar_t, Option*> abbreviations;
		std::map<std::wstring, Group*> groupIds;
		Arguments args;
		const arger::_Config* self = 0;
	};

	static inline void ValidateArguments(const detail::_Arguments& arguments, detail::ValidState& state, ValidState::Arguments& entry, std::wstring_view id);
	static inline void ValidateHelp(const detail::_Help& help, const std::wstring& who) {
		for (size_t i = 0; i < help.help.size(); ++i) {
			if (help.help[i].name.empty() || help.help[i].text.empty())
				throw arger::ConfigException{ L"Help name and help description of ", who, L" must not be empty." };
		}
	}
	static inline void ValidateType(const arger::Type& type, const std::wstring& who) {
		if (std::holds_alternative<arger::Enum>(type) && std::get<arger::Enum>(type).empty())
			throw arger::ConfigException{ L"Enum of ", who, L" must not be empty." };
	}
	static inline void ValidateOption(const arger::_Option& option, detail::ValidState& state) {
		if (option.name.empty())
			throw arger::ConfigException{ L"Option name must not be empty." };

		/* check if the name is unique */
		if (state.options.contains(option.name))
			throw arger::ConfigException{ L"Option with name [", option.name, L"] already exists." };
		ValidState::Option& entry = state.options[option.name];
		entry.self = &option;
		entry.payload = !option.payload.name.empty();

		/* check if the abbreviation is unique */
		if (option.abbreviation != 0) {
			if (state.abbreviations.contains(option.abbreviation))
				throw arger::ConfigException{ L"Option abbreviation [", option.abbreviation, L"] already exists." };
			state.abbreviations[option.abbreviation] = &entry;
		}

		/* validate the special-purpose flags */
		if (option.specialPurpose.help && option.specialPurpose.version)
			throw arger::ConfigException{ L"Option [", option.name, L"] cannot be help and version special purpose at once." };

		/* validate the payload */
		if (entry.payload)
			detail::ValidateType(option.payload.type, str::wd::Build(L"option [", option.name, L"]"));

		/* configure the limits */
		entry.minimum = option.require.minimum.value_or(0);
		if (option.require.maximum.has_value())
			entry.maximum = (*option.require.maximum == 0 ? 0 : std::max<size_t>(entry.minimum, *option.require.maximum));
		else
			entry.maximum = std::max<size_t>(entry.minimum, 1);
	}
	static inline void ValidateGroup(const arger::_Group& group, detail::ValidState& state, std::map<std::wstring, ValidState::Group>& super) {
		if (group.name.empty())
			throw arger::ConfigException{ L"Group name must not be empty." };
		const std::wstring& id = (group.id.empty() ? group.name : group.id);

		/* validate the name's uniqueness */
		if (super.contains(group.name))
			throw arger::ConfigException{ L"Group with name [", group.name, L"] already exists for given groups-set." };
		ValidState::Group& entry = super[group.name];
		entry.self = &group;
		entry.id = id;

		/* validate the arguments */
		detail::ValidateArguments(group, state, entry.args, id);

		/* check if the group-id is unique (only necessary if it does not contain sub-groups itself) */
		if (!entry.args.groups) {
			if (state.groupIds.contains(id))
				throw arger::ConfigException{ L"Group with id [", id, L"] already exists." };
			state.groupIds[id] = &entry;
		}

		/* validate the usages */
		for (const auto& option : group.use) {
			auto it = state.options.find(option);
			if (it == state.options.end())
				throw arger::ConfigException{ L"Group [", id, L"] uses undefined option [", option, L"]." };
			it->second.groups.insert(id);
		}

		/* validate the help attributes */
		detail::ValidateHelp(group, str::wd::Build(L"group [", id, L"]"));
	}
	static inline void ValidateArguments(const detail::_Arguments& arguments, detail::ValidState& state, ValidState::Arguments& entry, std::wstring_view id) {
		entry.self = &arguments;
		entry.groups = !arguments.groups.list.empty();
		entry.positionals = !arguments.positionals.empty();

		/* validate and configure the group name */
		if (arguments.groups.name.empty())
			entry.groupName = L"mode";
		else
			entry.groupName = arguments.groups.name;

		/* validate the groups */
		if (entry.positionals && entry.groups) {
			if (id.empty())
				throw arger::ConfigException{ L"Arguments cannot have positional arguments and sub-groups." };
			throw arger::ConfigException{ L"Group [", id, L"] cannot have positional arguments and sub-groups." };
		}
		if (entry.groups) {
			for (const auto& sub : arguments.groups.list)
				detail::ValidateGroup(sub, state, entry.sub);
			return;
		}

		/* configure the limits */
		entry.minimum = ((arguments.require.minimum.has_value() && entry.positionals) ? *arguments.require.minimum : arguments.positionals.size());
		if (arguments.require.maximum.has_value())
			entry.maximum = (*arguments.require.maximum == 0 ? 0 : std::max<size_t>({ entry.minimum, *arguments.require.maximum, arguments.positionals.size() }));
		else
			entry.maximum = std::max<size_t>(entry.minimum, arguments.positionals.size());

		/* validate the positionals */
		for (size_t i = 0; i < arguments.positionals.size(); ++i) {
			if (arguments.positionals[i].name.empty())
				throw arger::ConfigException{ L"Positional argument [", i, L"] of ", (id.empty() ? L"arguments" : str::wd::Build(L"group [", id, L"]")), L" must not have an empty name." };
			detail::ValidateType(arguments.positionals[i].type, id.empty() ? L"arguments" : str::wd::Build(L"groups [", id, L"] positional [", i, L"]"));
		}
	}
	static inline void ValidateConfig(const arger::_Config& config, detail::ValidState& state) {
		if (config.version.empty())
			throw arger::ConfigException{ L"Version must not be empty." };
		state.self = &config;

		/* validate the help attributes */
		detail::ValidateHelp(config, L"arguments");

		/* validate the options */
		for (const auto& option : config.options)
			detail::ValidateOption(option, state);

		/* validate the arguments */
		detail::ValidateArguments(config, state, state.args, L"");
	}
}
