/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"

namespace arger::detail {
	struct ValidGroup;

	struct ValidArguments {
		const detail::Arguments* args = 0;
		const detail::ValidArguments* super = 0;
		std::map<std::wstring, detail::ValidGroup> sub;
		std::wstring groupName;
		size_t minimum = 0;
		size_t maximum = 0;
		bool incomplete = false;
		bool nestedPositionals = false;
	};
	struct ValidOption {
		const arger::Option* option = 0;
		std::set<const detail::ValidGroup*> users;
		size_t minimum = 0;
		size_t maximum = 0;
		bool payload = false;
		bool restricted = false;
	};
	struct ValidConfig : public detail::ValidArguments {
		std::map<std::wstring, detail::ValidOption> options;
		std::map<wchar_t, detail::ValidOption*> abbreviations;
		std::map<std::wstring, detail::ValidGroup*> groupIds;
		const arger::Config* config = 0;
	};
	struct ValidGroup : public detail::ValidArguments {
		const arger::Group* group = 0;
		const detail::ValidGroup* parent = 0;
		std::wstring_view id;
	};

	inline void ValidateArguments(const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super);
	inline constexpr void ValidateHelp(const detail::Help& help, const std::wstring& who) {
		for (size_t i = 0; i < help.help.size(); ++i) {
			if (help.help[i].name.empty() || help.help[i].text.empty())
				throw arger::ConfigException{ L"Help name and help description of ", who, L" must not be empty." };
		}
	}
	inline constexpr void ValidateType(const arger::Type& type, const std::wstring& who) {
		if (std::holds_alternative<arger::Enum>(type) && std::get<arger::Enum>(type).empty())
			throw arger::ConfigException{ L"Enum of ", who, L" must not be empty." };
	}
	inline void ValidateOption(const arger::Option& option, detail::ValidConfig& state) {
		if (option.name.empty())
			throw arger::ConfigException{ L"Option name must not be empty." };
		if (option.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Option name must not start with a hypen." };

		/* check if the name is unique */
		if (state.options.contains(option.name))
			throw arger::ConfigException{ L"Option with name [", option.name, L"] already exists." };
		detail::ValidOption& entry = state.options[option.name];
		entry.option = &option;
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
		if ((option.specialPurpose.help || option.specialPurpose.version) && entry.payload)
			throw arger::ConfigException{ L"Option [", option.name, L"] cannot be a special purpose flag and carry a payload." };

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
	inline void ValidateGroup(const arger::Group& group, detail::ValidConfig& state, detail::ValidGroup* parent, detail::ValidArguments* super) {
		if (group.name.empty())
			throw arger::ConfigException{ L"Group name must not be empty." };
		if (group.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Group name must not start with a hypen." };
		const std::wstring& id = (group.id.empty() ? group.name : group.id);

		/* validate the name's uniqueness */
		if (super->sub.contains(group.name))
			throw arger::ConfigException{ L"Group with name [", group.name, L"] already exists for given groups-set." };
		detail::ValidGroup& entry = super->sub[group.name];
		entry.group = &group;
		entry.id = id;
		entry.parent = parent;
		entry.super = super;

		/* validate the arguments */
		detail::ValidateArguments(group, state, entry, &entry, super);

		/* check if the group-id is unique (only necessary if it does not contain sub-groups itself) */
		if (!entry.incomplete) {
			if (state.groupIds.contains(id))
				throw arger::ConfigException{ L"Group with id [", id, L"] already exists." };
			state.groupIds[id] = &entry;
		}
		else
			entry.id = {};

		/* validate the usages and register this group and all parents to them */
		for (const auto& option : entry.group->use) {
			auto it = state.options.find(option);
			if (it == state.options.end())
				throw arger::ConfigException{ L"Group [", entry.id, L"] uses undefined option [", option, L"]." };

			const detail::ValidGroup* walker = &entry;
			while (walker != 0) {
				it->second.users.insert(walker);
				walker = walker->parent;
			}
		}

		/* register this group to all options used by parents of this group */
		const detail::ValidGroup* walker = entry.parent;
		while (walker != 0) {
			for (const auto& option : walker->group->use)
				state.options.at(option).users.insert(&entry);
			walker = walker->parent;
		}

		/* validate the help attributes */
		detail::ValidateHelp(group, str::wd::Build(L"group [", id, L"]"));
	}
	inline void ValidateArguments(const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super) {
		entry.args = &arguments;
		entry.incomplete = !arguments.groups.list.empty();
		entry.nestedPositionals = !arguments.positionals.empty();

		/* validate and configure the group name */
		if (arguments.groups.name.empty())
			entry.groupName = L"mode";
		else
			entry.groupName = str::View{ arguments.groups.name }.lower();

		/* validate the groups */
		if (entry.incomplete && !arguments.positionals.empty()) {
			if (self == 0)
				throw arger::ConfigException{ L"Arguments cannot have positional arguments and sub-groups." };
			throw arger::ConfigException{ L"Group [", self->id, L"] cannot have positional arguments and sub-groups." };
		}
		if (entry.incomplete) {
			for (const auto& sub : arguments.groups.list)
				detail::ValidateGroup(sub, state, self, (self == 0 ? super : self));
			for (const auto& sub : entry.sub)
				entry.nestedPositionals = (entry.nestedPositionals || sub.second.nestedPositionals);
			return;
		}

		/* configure the limits */
		entry.minimum = ((arguments.require.minimum.has_value() && !arguments.positionals.empty()) ? *arguments.require.minimum : arguments.positionals.size());
		if (arguments.require.maximum.has_value())
			entry.maximum = (*arguments.require.maximum == 0 ? 0 : std::max<size_t>({ entry.minimum, *arguments.require.maximum, arguments.positionals.size() }));
		else
			entry.maximum = std::max<size_t>(entry.minimum, arguments.positionals.size());

		/* validate the positionals */
		for (size_t i = 0; i < arguments.positionals.size(); ++i) {
			if (arguments.positionals[i].name.empty())
				throw arger::ConfigException{ L"Positional argument [", i, L"] of ", (self == 0 ? L"arguments" : str::wd::Build(L"group [", self->id, L"]")), L" must not have an empty name." };
			detail::ValidateType(arguments.positionals[i].type, (self == 0 ? L"arguments" : str::wd::Build(L"groups [", self->id, L"] positional [", i, L"]")));
		}
	}
	inline void ValidateConfig(const arger::Config& config, detail::ValidConfig& state) {
		if (config.program.empty())
			throw arger::ConfigException{ L"Program must not be empty." };
		if (config.version.empty())
			throw arger::ConfigException{ L"Version must not be empty." };
		state.config = &config;

		/* validate the help attributes */
		detail::ValidateHelp(config, L"arguments");

		/* validate the options and arguments (validate the options before the arguments,
		*	as the arguments-usages will require the options to be already set) */
		for (const auto& option : config.options)
			detail::ValidateOption(option, state);
		detail::ValidateArguments(config, state, state, 0, &state);

		/* finalize the options by adding the null-group */
		for (auto& option : state.options) {
			option.second.restricted = !option.second.users.empty();
			option.second.users.insert(0);
		}
	}
}
