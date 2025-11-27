/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"
#include "arger-value.h"

namespace arger::detail {
	struct ValidEndpoint {
		const std::vector<detail::Positional>* positionals = nullptr;
		const std::vector<arger::Checker>* constraints = nullptr;
		const detail::Description* description = nullptr;
		size_t minimumEffective = 0;
		size_t minimumActual = 0;
		size_t maximum = 0;
		size_t id = 0;
	};
	struct ValidArguments {
		std::map<std::wstring, detail::ValidArguments> sub;
		std::map<wchar_t, detail::ValidArguments*> abbreviations;
		std::vector<detail::ValidEndpoint> endpoints;
		std::wstring groupName;
		const std::vector<arger::Checker>* constraints = nullptr;
		const detail::ValidArguments* super = nullptr;
		const detail::Group* group = nullptr;
		size_t depth = 0;
		bool nestedPositionals = false;
	};
	struct ValidOption {
		std::set<const detail::ValidArguments*> users;
		const detail::Option* option = nullptr;
		const detail::ValidArguments* owner = nullptr;
		size_t minimum = 0;
		size_t maximum = 0;
		bool payload = false;
	};
	struct ValidConfig : public detail::ValidArguments {
		const detail::Config* burned = nullptr;
		std::map<std::wstring, detail::ValidOption> options;
		std::map<wchar_t, detail::ValidOption*> abbreviations;
		std::map<size_t, detail::ValidOption*> optionIds;
		const detail::SpecialEntry* help = nullptr;
		const detail::SpecialEntry* version = nullptr;
	};

	inline bool CheckParent(const detail::ValidArguments* parent, const detail::ValidArguments* child) {
		/* check if the child can be above the parent */
		if (child->depth < parent->depth)
			return false;

		/* find the parent in the ancestor-chain of the child */
		while (child != 0) {
			if (child == parent)
				return true;
			child = child->super;
		}
		return false;
	}
	inline bool CheckAncestors(const detail::ValidArguments* a, const detail::ValidArguments* b) {
		/* find the other group in the younger of the two groups */
		if (a->depth < b->depth)
			return detail::CheckParent(a, b);
		return detail::CheckParent(b, a);
	}
	inline bool CheckUsage(const detail::ValidOption* option, const detail::ValidArguments* group) {
		for (const auto& user : option->users) {
			if (detail::CheckAncestors(user, group))
				return true;
		}
		return false;
	}

	inline constexpr void ValidateDescription(detail::ValidConfig& state, const detail::Description& description) {
		if (description.description.normal.empty() && !description.description.reduced.empty())
			throw arger::ConfigException{ L"Reduced description requires normal description as well." };
		if (state.help == nullptr || !state.help->reducible)
			throw arger::ConfigException{ L"Reduced description requires reduced help to be possible." };

	}
	inline constexpr void ValidateInformation(detail::ValidConfig& state, const detail::Information& information) {
		for (const auto& info : information.information) {
			if (info.name.empty() || info.text.empty())
				throw arger::ConfigException{ L"Information name and description must not be empty." };
			if (info.reduced && (state.help == nullptr || !state.help->reducible))
				throw arger::ConfigException{ L"Reduced information requires reduced help to be possible." };
			if (!info.reduced && !info.normal)
				throw arger::ConfigException{ L"Information must either be bound to the normal or reduced menu." };
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
		if (state.help != nullptr) {
			if (name == state.help->name)
				throw arger::ConfigException{ L"Name clashes with help entry name." };
			if (abbreviation != 0 && abbreviation == state.help->abbreviation)
				throw arger::ConfigException{ L"Abbreviation clashes with help entry abbreviation." };
		}
		if (state.version != nullptr) {
			if (name == state.version->name)
				throw arger::ConfigException{ L"Name clashes with version entry name." };
			if (abbreviation != 0 && abbreviation == state.version->abbreviation)
				throw arger::ConfigException{ L"Abbreviation clashes with version entry abbreviation." };
		}
	}

	inline void ValidateEndpoint(detail::ValidConfig& state, detail::ValidArguments& entry, const detail::Arguments* args, const detail::Endpoint* endpoint) {
		const std::vector<detail::Positional>& positionals = (args == nullptr ? endpoint->positionals : args->positionals);
		std::optional<size_t> minimum = (args == nullptr ? endpoint->require : args->require).minimum;
		std::optional<size_t> maximum = (args == nullptr ? endpoint->require : args->require).maximum;

		/* setup the new endpoint (all-children is ignored for end-points as they do not have children) */
		detail::ValidEndpoint& next = entry.endpoints.emplace_back();
		next.positionals = &positionals;
		next.constraints = (args == nullptr ? &endpoint->constraints : nullptr);
		next.description = (args == nullptr ? endpoint : nullptr);
		next.id = (args == nullptr ? endpoint->id : args->endpointId);

		/* validate the description */
		if (next.description != nullptr)
			detail::ValidateDescription(state, *next.description);

		/* validate and configure the minimum */
		if (minimum.has_value() && positionals.empty())
			throw arger::ConfigException{ L"Minimum requires at least one positional to be defined." };
		next.minimumActual = minimum.value_or(positionals.size());

		/* validate and configure the maximum */
		if (!maximum.has_value())
			next.maximum = std::max<size_t>(next.minimumActual, positionals.size());
		else if (*maximum < next.minimumActual)
			next.maximum = 0;
		else if (*maximum < positionals.size() && *maximum > 0)
			throw arger::ConfigException{ L"Maximum must be at least the number of positionals." };
		else
			next.maximum = *maximum;

		/* patch the minimum to be aware of default parameters (minimum will be zero for empty positionals) */
		next.minimumEffective = next.minimumActual;
		while (next.minimumEffective > 0 && positionals[std::min<size_t>(positionals.size(), next.minimumEffective) - 1].defValue.has_value())
			--next.minimumEffective;

		/* validate the positionals */
		for (size_t i = 0; i < positionals.size(); ++i) {
			/* validate the name and type */
			if (positionals[i].name.empty())
				throw arger::ConfigException{ L"Positional argument must not have an empty name." };
			detail::ValidateType(positionals[i].type);

			/* validate the default-value */
			if (!positionals[i].defValue.has_value())
				continue;
			detail::ValidateDefValue(positionals[i].type, positionals[i].defValue.value());
			if (i < next.minimumEffective)
				throw arger::ConfigException{ L"All positionals up to the minimum must be defaulted once one is defaulted." };

			/* validat ethe description */
			detail::ValidateDescription(state, positionals[i]);
		}
	}
	inline void ValidateOption(detail::ValidConfig& state, const detail::Option& option, const detail::ValidArguments* owner) {
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

		/* check if the name or abbreviation clashes with the help/version entries (for options only necessary for programs) */
		if (!state.burned->program.empty())
			detail::ValidateSpecialEntry(state, option.name, option.abbreviation);

		/* validate the description */
		detail::ValidateDescription(state, option);

		/* validate the payload */
		if (entry.payload)
			detail::ValidateType(option.payload.type);
		else if (option.require.minimum.has_value() || option.require.maximum.has_value())
			throw arger::ConfigException{ L"Flags cannot have requirements defined." };

		/* configure the minimum */
		entry.minimum = option.require.minimum.value_or(0);

		/* validate and configure the maximum */
		if (!option.require.maximum.has_value())
			entry.maximum = std::max<size_t>(entry.minimum, 1);
		else if (*option.require.maximum < entry.minimum)
			entry.maximum = 0;
		else
			entry.maximum = *option.require.maximum;

		/* validate the default-values */
		if (entry.payload && !option.payload.defValue.empty()) {
			if (option.payload.defValue.size() < entry.minimum)
				throw arger::ConfigException{ L"Default values for option must not violate its own minimum requirements." };
			if (entry.maximum > 0 && option.payload.defValue.size() > entry.maximum)
				throw arger::ConfigException{ L"Default values for option must not violate its own maximum requirements." };
			for (const auto& value : option.payload.defValue)
				detail::ValidateDefValue(option.payload.type, value);
		}
	}
	inline void ValidateArguments(detail::ValidConfig& state, const detail::Arguments& arguments, const detail::Group* group, detail::ValidArguments& entry, detail::ValidArguments* super) {
		/* populate the entry */
		entry.constraints = &arguments.constraints;
		entry.nestedPositionals = !arguments.positionals.empty();
		entry.super = super;
		entry.depth = (super == nullptr ? 0 : super->depth + 1);
		entry.group = group;

		/* validate and configure the group name */
		if (arguments.groups.name.empty())
			entry.groupName = L"mode";
		else
			entry.groupName = str::View{ arguments.groups.name }.lower();

		/* validate the description */
		detail::ValidateDescription(state, arguments);

		/* validate the groups */
		if (!arguments.groups.list.empty()) {
			if (!arguments.positionals.empty() || !arguments.endpoints.empty())
				throw arger::ConfigException{ L"Groups and positional arguments cannot be used in conjunction." };

			/* register all groups */
			for (const auto& sub : arguments.groups.list) {
				if (sub.name.size() <= 1)
					throw arger::ConfigException{ L"Group name must at least be two characters long." };
				if (sub.name.starts_with(L"-"))
					throw arger::ConfigException{ L"Group name must not start with a hypen." };

				/* validate the name's uniqueness */
				if (entry.sub.contains(sub.name))
					throw arger::ConfigException{ L"Group names within a sub-group must be unique." };
				detail::ValidArguments& next = entry.sub[sub.name];

				/* check if the abbreviation is unique */
				if (sub.abbreviation != 0) {
					if (entry.abbreviations.contains(sub.abbreviation))
						throw arger::ConfigException{ L"Group abbreviations within a sub-group must be unique." };
					entry.abbreviations[sub.abbreviation] = &next;
				}

				/* check if the name or abbreviation clashes with the help/version entries (for groups only necessary for menus) */
				if (state.burned->program.empty())
					detail::ValidateSpecialEntry(state, sub.name, sub.abbreviation);

				/* validate the arguments */
				detail::ValidateArguments(state, sub, &sub, next, &entry);
				entry.nestedPositionals = (entry.nestedPositionals || next.nestedPositionals);

				/* register all new options */
				for (const auto& option : sub.options)
					detail::ValidateOption(state, option, &next);

				/* validate the information attributes */
				detail::ValidateInformation(state, sub);
			}
			return;
		}

		/* check if an implicit endpoint needs to be added */
		if (arguments.endpoints.empty())
			detail::ValidateEndpoint(state, entry, &arguments, nullptr);

		/* validate all explicit endpoints */
		else {
			if (!arguments.positionals.empty() || arguments.require.minimum.has_value() || arguments.require.maximum.has_value())
				throw arger::ConfigException{ L"Implicit and explicit endpoints cannot be used in conjunction." };
			for (size_t i = 0; i < arguments.endpoints.size(); ++i)
				detail::ValidateEndpoint(state, entry, nullptr, &arguments.endpoints[i]);
		}

		/* sort all endpoints and ensure that they can be uniquely differentiated */
		std::sort(entry.endpoints.begin(), entry.endpoints.end(), [](const detail::ValidEndpoint& a, const detail::ValidEndpoint& b) -> bool {
			return (a.minimumEffective < b.minimumEffective);
			});
		for (size_t i = 1; i < entry.endpoints.size(); ++i) {
			if (entry.endpoints[i - 1].maximum >= entry.endpoints[i].minimumEffective)
				throw arger::ConfigException{ L"Endpoint positional effective requirement counts must not overlap in order to ensure each endpoint can be matched uniquely." };
		}
	}
	inline void ValidateFinalizeArguments(detail::ValidConfig& state, const detail::ValidArguments& group) {
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
			detail::ValidateFinalizeArguments(state, child);
	}
	inline void ValidateConfig(const arger::Config& config, detail::ValidConfig& state) {
		/* burn the config and reset the state (validated state must not
		*	outlive original config and config must not be modified inbetween) */
		state.burned = &detail::ConfigBurner::GetBurned(config);
		state.options.clear();
		state.abbreviations.clear();
		state.optionIds.clear();
		state.help = nullptr;
		state.version = nullptr;

		/* validate the special-purpose entries attributes */
		if (!state.burned->special.help.name.empty()) {
			if (state.burned->special.help.name.size() <= 1)
				throw arger::ConfigException{ L"Help entry name must at least be two characters long." };
			if (state.burned->special.version.name == state.burned->special.help.name)
				throw arger::ConfigException{ L"Help entry and version entry cannot have the same name." };
			if (state.burned->special.help.reducible && state.burned->special.help.abbreviation == 0)
				throw arger::ConfigException{ L"Reducible help entry requires a defined abbreviarion." };
			if (!state.burned->special.version.name.empty()) {
				if (state.burned->special.help.abbreviation != 0 && state.burned->special.help.abbreviation == state.burned->special.version.abbreviation)
					throw arger::ConfigException{ L"Help entry and version entry cannot have the same abbreviation." };
			}
			state.help = &state.burned->special.help;
			detail::ValidateDescription(state, state.burned->special.help);
		}
		if (!state.burned->special.version.name.empty()) {
			if (state.burned->special.version.name.size() <= 1)
				throw arger::ConfigException{ L"Version entry name must at least be two characters long." };
			if (state.burned->version.empty())
				throw arger::ConfigException{ L"Version string must be set when using a version entry." };
			state.version = &state.burned->special.version;
			detail::ValidateDescription(state, state.burned->special.version);
		}

		/* validate the information attributes */
		detail::ValidateInformation(state, *state.burned);

		/* validate the options and arguments */
		for (const auto& option : state.burned->options)
			detail::ValidateOption(state, option, &state);
		detail::ValidateArguments(state, *state.burned, nullptr, state, nullptr);

		/* post-validate all groups after all groups and flags have been loaded */
		for (const auto& [_, group] : state.sub)
			detail::ValidateFinalizeArguments(state, group);

		/* finalize all options by adding the owner to all non-restricted options */
		for (auto& [name, option] : state.options) {
			if (option.users.empty())
				option.users.insert(option.owner);
		}
	}
}
