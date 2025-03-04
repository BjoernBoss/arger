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
		std::map<wchar_t, detail::ValidGroup*> abbreviations;
		std::wstring groupName;
		size_t minimum = 0;
		size_t maximum = 0;
		bool incomplete = false;
		bool nestedPositionals = false;
	};
	struct ValidOption {
		const arger::Option* option = 0;
		std::set<const detail::ValidGroup*> users;
		const detail::ValidGroup* owner = 0;
		size_t minimum = 0;
		size_t maximum = 0;
		bool payload = false;
	};
	struct ValidConfig : public detail::ValidArguments {
		std::map<std::wstring, detail::ValidOption> options;
		std::map<wchar_t, detail::ValidOption*> abbreviations;
		const arger::Config* config = 0;
		const detail::SpecialEntry* help = 0;
		const detail::SpecialEntry* version = 0;
	};
	struct ValidGroup : public detail::ValidArguments {
		const arger::Group* group = 0;
		const detail::ValidGroup* parent = 0;
		size_t depth = 0;
	};

	inline bool CheckParent(const detail::ValidGroup* parent, const detail::ValidGroup* child) {
		if (parent == 0)
			return true;
		if (child == 0)
			return false;

		/* check if the child can be above the parent */
		if (child->depth < parent->depth)
			return false;

		/* find the parent in the ancestor-chain of the child */
		while (child != 0) {
			if (child == parent)
				return true;
			child = child->parent;
		}
		return false;
	}
	inline bool CheckAncestors(const detail::ValidGroup* a, const detail::ValidGroup* b) {
		if (a == 0 || b == 0)
			return true;

		/* find the other group in the younger of the two groups */
		if (a->depth < b->depth)
			return detail::CheckParent(a, b);
		return detail::CheckParent(b, a);
	}
	inline bool CheckUsage(const detail::ValidOption* option, const detail::ValidGroup* group) {
		for (const auto& user : option->users) {
			if (detail::CheckAncestors(user, group))
				return true;
		}
		return false;
	}

	inline void ValidateArguments(const arger::Config& config, const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super, bool menu);
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
	inline constexpr void ValidateDefValue(const arger::Type& type, const arger::Value& value, const std::wstring& who) {
		/* check if the value must be an enum */
		if (std::holds_alternative<arger::Enum>(type)) {
			const arger::Enum& allowed = std::get<arger::Enum>(type);
			if (value.isStr() && allowed.count(value.str()) != 0)
				return;
			throw arger::ConfigException{ L"Default value of ", who, L" must be a valid enum for the given type." };
		}

		/* validate the expected default type (value automatically performs conversion) */
		switch (std::get<arger::Primitive>(type)) {
		case arger::Primitive::boolean:
			if (!value.isBool())
				throw arger::ConfigException{ L"Default value of ", who, L" is expected to be a boolean." };
			break;
		case arger::Primitive::real:
			if (!value.isReal())
				throw arger::ConfigException{ L"Default value of ", who, L" is expected to be a real." };
			break;
		case arger::Primitive::inum:
			if (!value.isINum())
				throw arger::ConfigException{ L"Default value of ", who, L" is expected to be a signed integer." };
			break;
		case arger::Primitive::unum:
			if (!value.isUNum())
				throw arger::ConfigException{ L"Default value of ", who, L" is expected to be an unsigned integer." };
			break;
		case arger::Primitive::any:
			break;
		}
	}
	inline constexpr void ValidateSpecialEntry(const detail::ValidConfig& state, const std::wstring& name, wchar_t abbreviation, const std::wstring& who) {
		if (state.help != 0) {
			if (name == state.help->name)
				throw arger::ConfigException{ who, L" with name [", name, L"] clashes with help entry name." };
			if (abbreviation != 0 && abbreviation == state.help->abbreviation)
				throw arger::ConfigException{ who, L" abbreviation [", abbreviation, L"] clashes with help entry abbreviation." };
		}
		if (state.version != 0) {
			if (name == state.version->name)
				throw arger::ConfigException{ who, L" with name [", name, L"] clashes with version entry name." };
			if (abbreviation != 0 && abbreviation == state.version->abbreviation)
				throw arger::ConfigException{ who, L" abbreviation [", abbreviation, L"] clashes with version entry abbreviation." };
		}
	}
	inline void ValidateOption(const arger::Config& config, const arger::Option& option, detail::ValidConfig& state, const detail::ValidGroup* owner, bool menu) {
		if (option.name.size() <= 1)
			throw arger::ConfigException{ L"Option name must at least be two characters long." };
		if (option.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Option name must not start with a hypen." };

		/* check if the name is unique */
		if (state.options.contains(option.name))
			throw arger::ConfigException{ L"Option with name [", option.name, L"] already exists." };
		detail::ValidOption& entry = state.options[option.name];
		entry.option = &option;
		entry.payload = !option.payload.name.empty();
		entry.owner = owner;

		/* add the owner as first user of the group */
		if (owner != 0)
			entry.users.insert(owner);

		/* check if the abbreviation is unique */
		if (option.abbreviation != 0) {
			if (state.abbreviations.contains(option.abbreviation))
				throw arger::ConfigException{ L"Option abbreviation [", option.abbreviation, L"] already exists." };
			state.abbreviations[option.abbreviation] = &entry;
		}

		/* check if the name or abbreviation clashes with the help/version entries */
		if (!menu)
			detail::ValidateSpecialEntry(state, option.name, option.abbreviation, L"Option");

		/* validate the payload */
		std::wstring whoSelf = str::wd::Build(L"option [", option.name, L']');
		if (entry.payload)
			detail::ValidateType(option.payload.type, whoSelf);

		/* configure the limits */
		entry.minimum = option.require.minimum.value_or(0);
		if (option.require.maximum.has_value())
			entry.maximum = (*option.require.maximum == 0 ? 0 : std::max<size_t>(entry.minimum, *option.require.maximum));
		else
			entry.maximum = std::max<size_t>(entry.minimum, 1);

		/* validate the default-values */
		if (entry.payload && !option.payload.defValue.empty()) {
			if (option.payload.defValue.size() < entry.minimum)
				throw arger::ConfigException{ L"Default values for option [", option.name, L"] must not violate its own minimum requirements" };
			if (entry.maximum > 0 && option.payload.defValue.size() > entry.maximum)
				throw arger::ConfigException{ L"Default values for option [", option.name, L"] must not violate its own maximum requirements" };
			for (const auto& value : option.payload.defValue)
				detail::ValidateDefValue(option.payload.type, value, whoSelf);
		}
	}
	inline void ValidateGroup(const arger::Config& config, const arger::Group& group, detail::ValidConfig& state, detail::ValidGroup* parent, detail::ValidArguments* super, bool menu) {
		if (group.name.size() <= 1)
			throw arger::ConfigException{ L"Group name must at least be two characters long." };
		if (group.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Group name must not start with a hypen." };

		/* validate the name's uniqueness */
		if (super->sub.contains(group.name))
			throw arger::ConfigException{ L"Group with name [", group.name, L"] already exists for given groups-set." };
		detail::ValidGroup& entry = super->sub[group.name];
		entry.group = &group;
		entry.parent = parent;
		entry.super = super;
		entry.depth = (parent == 0 ? 0 : parent->depth + 1);

		/* check if the abbreviation is unique */
		if (group.abbreviation != 0) {
			if (super->abbreviations.contains(group.abbreviation))
				throw arger::ConfigException{ L"Abbreviation [", group.abbreviation, L"] for group with name [", group.name, L"] already exists." };
			super->abbreviations[group.abbreviation] = &entry;
		}

		/* check if the name or abbreviation clashes with the help/version entries */
		if (menu)
			detail::ValidateSpecialEntry(state, group.name, group.abbreviation, L"Group");

		/* validate the arguments */
		detail::ValidateArguments(config, group, state, entry, &entry, super, menu);

		/* register all new options */
		for (const auto& option : group.options)
			detail::ValidateOption(config, option, state, &entry, menu);

		/* validate the help attributes */
		detail::ValidateHelp(group, str::wd::Build(L"group [", group.name, L"]"));
	}
	inline void ValidateArguments(const arger::Config& config, const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super, bool menu) {
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
			throw arger::ConfigException{ L"Group [", self->group->name, L"] cannot have positional arguments and sub-groups." };
		}
		if (entry.incomplete) {
			for (const auto& sub : arguments.groups.list)
				detail::ValidateGroup(config, sub, state, self, (self == 0 ? super : self), menu);
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
			/* validate the name and type */
			if (arguments.positionals[i].name.empty())
				throw arger::ConfigException{ L"Positional argument [", i, L"] of ", (self == 0 ? L"arguments" : str::wd::Build(L"group [", self->group->name, L"]")), L" must not have an empty name." };
			std::wstring whoSelf = (self == 0 ? str::wd::Build(L"arguments positional [", i, L']') : str::wd::Build(L"groups [", self->group->name, L"] positional [", i, L']'));
			detail::ValidateType(arguments.positionals[i].type, whoSelf);

			/* validate the default-value */
			if (arguments.positionals[i].defValue.has_value())
				detail::ValidateDefValue(arguments.positionals[i].type, arguments.positionals[i].defValue.value(), whoSelf);
		}
	}
	inline void ValidateFinalizedGroups(detail::ValidConfig& state, const detail::ValidGroup& group) {
		/* validate that used options are defined and usable */
		for (const auto& option : group.group->use) {
			/* check if the used option exists */
			auto it = state.options.find(option);
			if (it == state.options.end())
				throw arger::ConfigException{ L"Group [", group.group->name, L"] uses undefined option [", option, L"]." };

			/* check if the used option can be used by this group */
			if (!detail::CheckParent(it->second.owner, &group))
				throw arger::ConfigException{ L"Group [", group.group->name, L"] cannot use option [", option, L"] from another group." };

			/* add itself to the users of the group */
			it->second.users.insert(&group);
		}

		/* validate all children */
		for (const auto& [_, child] : group.sub)
			detail::ValidateFinalizedGroups(state, child);
	}
	inline void ValidateConfig(const arger::Config& config, detail::ValidConfig& state, bool menu) {
		if (menu && !config.program.empty())
			throw arger::ConfigException{ L"Menu cannot have a program name." };
		if (!menu && config.program.empty())
			throw arger::ConfigException{ L"Configuration must have a program name." };
		state.config = &config;

		/* validate the special-purpose entries attributes */
		if (!config.special.help.name.empty()) {
			if (config.special.help.name.size() <= 1)
				throw arger::ConfigException{ L"Help entry name must at least be two characters long." };
			if (config.special.version.name == config.special.help.name)
				throw arger::ConfigException{ L"Help entry and version entry cannot both have the name [", config.special.help.name, L"]." };
			if (!config.special.version.name.empty()) {
				if (config.special.help.abbreviation != 0 && config.special.help.abbreviation == config.special.version.abbreviation)
					throw arger::ConfigException{ L"Help entry and version entry cannot both have the abbreviation [", config.special.help.abbreviation, L"]." };
			}
			state.help = &config.special.help;
		}
		if (!config.special.version.name.empty()) {
			if (config.special.version.name.size() <= 1)
				throw arger::ConfigException{ L"Version entry name must at least be two characters long." };
			state.version = &config.special.version;
		}

		/* validate the help attributes */
		detail::ValidateHelp(config, L"arguments");

		/* validate the options and arguments */
		for (const auto& option : config.options)
			detail::ValidateOption(config, option, state, 0, menu);
		detail::ValidateArguments(config, config, state, state, 0, &state, menu);

		/* post-validate all groups after all groups and flags have been loaded */
		for (const auto& [_, group] : state.sub)
			detail::ValidateFinalizedGroups(state, group);

		/* finalize all options by adding the root-group to all non-restricted options */
		for (auto& [name, option] : state.options) {
			if (option.users.empty())
				option.users.insert(0);
		}
	}
}
