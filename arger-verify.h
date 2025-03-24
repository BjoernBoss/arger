/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"
#include "arger-value.h"

namespace arger::detail {
	struct ValidGroup;

	struct ValidEndpoint {
		const std::vector<detail::Positionals::Entry>* positionals = 0;
		const std::vector<arger::Checker>* constraints = 0;
		const std::wstring* description = 0;
		size_t minimum = 0;
		size_t maximum = 0;
		size_t id = 0;
	};
	struct ValidArguments {
		const std::vector<arger::Checker>* constraints = 0;
		const detail::ValidArguments* super = 0;
		std::map<std::wstring, detail::ValidGroup> sub;
		std::map<wchar_t, detail::ValidGroup*> abbreviations;
		std::vector<detail::ValidEndpoint> endpoints;
		std::wstring groupName;
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
		std::map<size_t, detail::ValidOption*> optionIds;
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

	inline constexpr void ValidateHelp(const detail::Help& help) {
		for (size_t i = 0; i < help.help.size(); ++i) {
			if (help.help[i].name.empty() || help.help[i].text.empty())
				throw arger::ConfigException{ L"Help name and help description must not be empty." };
		}
	}
	inline constexpr void ValidateType(const arger::Type& type) {
		if (std::holds_alternative<arger::Enum>(type) && std::get<arger::Enum>(type).empty())
			throw arger::ConfigException{ L"Enum must not be empty." };
	}
	inline constexpr void ValidateDefValue(const arger::Type& type, const arger::Value& value) {
		/* check if the value must be an enum */
		if (std::holds_alternative<arger::Enum>(type)) {
			const arger::Enum& allowed = std::get<arger::Enum>(type);
			if (value.isStr() && std::find_if(allowed.begin(), allowed.end(), [&](const arger::EnumEntry& e) { return e.name == value.str(); }) != allowed.end())
				return;
			throw arger::ConfigException{ L"Default value must be a valid enum-string for the given type." };
		}

		/* validate the expected default type (value automatically performs conversion) */
		switch (std::get<arger::Primitive>(type)) {
		case arger::Primitive::boolean:
			if (!value.isBool())
				throw arger::ConfigException{ L"Default value is expected to be a boolean." };
			break;
		case arger::Primitive::real:
			if (!value.isReal())
				throw arger::ConfigException{ L"Default value is expected to be a real." };
			break;
		case arger::Primitive::inum:
			if (!value.isINum())
				throw arger::ConfigException{ L"Default value is expected to be a signed integer." };
			break;
		case arger::Primitive::unum:
			if (!value.isUNum())
				throw arger::ConfigException{ L"Default value is expected to be an unsigned integer." };
			break;
		case arger::Primitive::any:
			break;
		}
	}
	inline constexpr void ValidateSpecialEntry(const detail::ValidConfig& state, const std::wstring& name, wchar_t abbreviation) {
		if (state.help != 0) {
			if (name == state.help->name)
				throw arger::ConfigException{ L"Name clashes with help entry name." };
			if (abbreviation != 0 && abbreviation == state.help->abbreviation)
				throw arger::ConfigException{ L"Abbreviation clashes with help entry abbreviation." };
		}
		if (state.version != 0) {
			if (name == state.version->name)
				throw arger::ConfigException{ L"Name clashes with version entry name." };
			if (abbreviation != 0 && abbreviation == state.version->abbreviation)
				throw arger::ConfigException{ L"Abbreviation clashes with version entry abbreviation." };
		}
	}

	inline void ValidateEndpoint(detail::ValidArguments& entry, const detail::Arguments* args, const arger::Endpoint* endpoint) {
		const std::vector<detail::Positionals::Entry>& positionals = (args == 0 ? endpoint->positionals : args->positionals);
		std::optional<size_t> minimum = (args == 0 ? endpoint->require : args->require).minimum;
		std::optional<size_t> maximum = (args == 0 ? endpoint->require : args->require).maximum;

		/* setup the new endpoint */
		detail::ValidEndpoint& next = entry.endpoints.emplace_back();
		next.positionals = &positionals;
		next.constraints = (args == 0 ? &endpoint->constraints : 0);
		next.description = (args == 0 ? &endpoint->description : 0);
		next.id = (args == 0 ? endpoint->id : args->endpointId);

		/* configure the limits */
		next.minimum = ((minimum.has_value() && !positionals.empty()) ? *minimum : positionals.size());
		if (maximum.has_value())
			next.maximum = (*maximum == 0 ? 0 : std::max<size_t>({ next.minimum, *maximum, positionals.size() }));
		else
			next.maximum = std::max<size_t>(next.minimum, positionals.size());

		/* patch the minimum to be aware of default parameters (minimum will be zero for empty positionals) */
		while (next.minimum > 0 && positionals[std::min<size_t>(positionals.size(), next.minimum) - 1].defValue.has_value())
			--next.minimum;

		/* validate the positionals */
		for (size_t i = 0; i < positionals.size(); ++i) {
			/* validate the name and type */
			if (positionals[i].name.empty())
				throw arger::ConfigException{ L"Positional argument must not have an empty name." };
			detail::ValidateType(positionals[i].type);

			/* validate the default-value */
			if (positionals[i].defValue.has_value())
				detail::ValidateDefValue(positionals[i].type, positionals[i].defValue.value());
		}
	}
	inline void ValidateArguments(const arger::Config& config, const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super, bool menu);
	inline void ValidateOption(const arger::Config& config, const arger::Option& option, detail::ValidConfig& state, const detail::ValidGroup* owner, bool menu) {
		if (option.name.size() <= 1)
			throw arger::ConfigException{ L"Option name must at least be two characters long." };
		if (option.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Option name must not start with a hypen." };

		/* check if the name is unique */
		if (state.options.contains(option.name))
			throw arger::ConfigException{ L"Option names must be unique." };
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
				throw arger::ConfigException{ L"Option abbreviations must be unique." };
			state.abbreviations[option.abbreviation] = &entry;
		}

		/* check if the id is unique */
		if (state.optionIds.contains(option.id))
			throw arger::ConfigException{ L"Option ids must be unique." };
		state.optionIds.insert({ option.id, &entry });

		/* check if the name or abbreviation clashes with the help/version entries */
		if (!menu)
			detail::ValidateSpecialEntry(state, option.name, option.abbreviation);

		/* validate the payload */
		if (entry.payload)
			detail::ValidateType(option.payload.type);

		/* configure the limits */
		entry.minimum = option.require.minimum.value_or(0);
		if (option.require.maximum.has_value())
			entry.maximum = (*option.require.maximum == 0 ? 0 : std::max<size_t>(entry.minimum, *option.require.maximum));
		else
			entry.maximum = std::max<size_t>(entry.minimum, 1);

		/* validate the default-values */
		if (entry.payload && !option.payload.defValue.empty()) {
			if (option.payload.defValue.size() < entry.minimum)
				throw arger::ConfigException{ L"Default values for option must not violate its own minimum requirements" };
			if (entry.maximum > 0 && option.payload.defValue.size() > entry.maximum)
				throw arger::ConfigException{ L"Default values for option must not violate its own maximum requirements" };
			for (const auto& value : option.payload.defValue)
				detail::ValidateDefValue(option.payload.type, value);
		}
	}
	inline void ValidateGroup(const arger::Config& config, const arger::Group& group, detail::ValidConfig& state, detail::ValidGroup* parent, detail::ValidArguments* super, bool menu) {
		if (group.name.size() <= 1)
			throw arger::ConfigException{ L"Group name must at least be two characters long." };
		if (group.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Group name must not start with a hypen." };

		/* validate the name's uniqueness */
		if (super->sub.contains(group.name))
			throw arger::ConfigException{ L"Group names within a sub-group must be unique." };
		detail::ValidGroup& entry = super->sub[group.name];
		entry.group = &group;
		entry.parent = parent;
		entry.super = super;
		entry.depth = (parent == 0 ? 0 : parent->depth + 1);

		/* check if the abbreviation is unique */
		if (group.abbreviation != 0) {
			if (super->abbreviations.contains(group.abbreviation))
				throw arger::ConfigException{ L"Group abbreviations within a sub-group must be unique." };
			super->abbreviations[group.abbreviation] = &entry;
		}

		/* check if the name or abbreviation clashes with the help/version entries */
		if (menu)
			detail::ValidateSpecialEntry(state, group.name, group.abbreviation);

		/* validate the arguments */
		detail::ValidateArguments(config, group, state, entry, &entry, super, menu);

		/* register all new options */
		for (const auto& option : group.options)
			detail::ValidateOption(config, option, state, &entry, menu);

		/* validate the help attributes */
		detail::ValidateHelp(group);
	}
	inline void ValidateArguments(const arger::Config& config, const detail::Arguments& arguments, detail::ValidConfig& state, detail::ValidArguments& entry, detail::ValidGroup* self, detail::ValidArguments* super, bool menu) {
		entry.constraints = &arguments.constraints;
		entry.nestedPositionals = !arguments.positionals.empty();

		/* validate and configure the group name */
		if (arguments.groups.name.empty())
			entry.groupName = L"mode";
		else
			entry.groupName = str::View{ arguments.groups.name }.lower();

		/* validate the groups */
		if (!arguments.groups.list.empty() && (!arguments.positionals.empty() || !arguments.endpoints.empty())) {
			if (self == 0)
				throw arger::ConfigException{ L"Arguments cannot have positional arguments and sub-groups." };
			throw arger::ConfigException{ L"Group cannot have positional arguments and sub-groups." };
		}
		if (!arguments.groups.list.empty()) {
			for (const auto& sub : arguments.groups.list)
				detail::ValidateGroup(config, sub, state, self, (self == 0 ? super : self), menu);
			for (const auto& sub : entry.sub)
				entry.nestedPositionals = (entry.nestedPositionals || sub.second.nestedPositionals);
			return;
		}

		/* check if an implicit endpoint needs to be added */
		if (arguments.endpoints.empty())
			detail::ValidateEndpoint(entry, &arguments, 0);

		/* validate all explicit endpoints */
		else {
			if (!arguments.positionals.empty() || arguments.require.minimum.has_value() || arguments.require.maximum.has_value())
				throw arger::ConfigException{ L"Implicit and explicit endpoints cannot be used in conjunction." };
			for (size_t i = 0; i < arguments.endpoints.size(); ++i)
				detail::ValidateEndpoint(entry, 0, &arguments.endpoints[i]);
		}

		/* sort all endpoints and ensure that they can be uniquely differentiated */
		std::sort(entry.endpoints.begin(), entry.endpoints.end(), [](const detail::ValidEndpoint& a, const detail::ValidEndpoint& b) -> bool {
			return (a.minimum < b.minimum);
			});
		for (size_t i = 1; i < entry.endpoints.size(); ++i) {
			if (entry.endpoints[i - 1].maximum >= entry.endpoints[i].minimum)
				throw arger::ConfigException{ L"Endpoint positional requirement counts must not overlap in order to ensure each endpoint can be matched uniquely." };
		}
	}
	inline void ValidateFinalizedGroups(detail::ValidConfig& state, const detail::ValidGroup& group) {
		/* validate that used options are defined and usable */
		for (size_t option : group.group->use) {
			auto it = state.optionIds.find(option);
			if (it == state.optionIds.end())
				throw arger::ConfigException{ L"Group uses undefined option." };

			/* check if the used option can be used by this group */
			if (!detail::CheckParent(it->second->owner, &group))
				throw arger::ConfigException{ L"Group cannot use options from another group." };

			/* add itself to the users of the group */
			it->second->users.insert(&group);
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
				throw arger::ConfigException{ L"Help entry and version entry cannot have the same name." };
			if (!config.special.version.name.empty()) {
				if (config.special.help.abbreviation != 0 && config.special.help.abbreviation == config.special.version.abbreviation)
					throw arger::ConfigException{ L"Help entry and version entry cannot have the same abbreviation." };
			}
			state.help = &config.special.help;
		}
		if (!config.special.version.name.empty()) {
			if (config.special.version.name.size() <= 1)
				throw arger::ConfigException{ L"Version entry name must at least be two characters long." };
			state.version = &config.special.version;
		}

		/* validate the help attributes */
		detail::ValidateHelp(config);

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
