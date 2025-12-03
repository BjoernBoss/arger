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
		bool hidden = false;
	};
	struct ValidArguments {
		std::map<std::wstring, detail::ValidArguments> sub;
		std::map<wchar_t, detail::ValidArguments*> abbreviations;
		std::vector<detail::ValidEndpoint> endpoints;
		std::wstring groupName;
		const std::vector<arger::Checker>* constraints = nullptr;
		const detail::ValidArguments* super = nullptr;
		const detail::Arguments* args = nullptr;
		const detail::Group* group = nullptr;
		size_t depth = 0;
		bool nestedPositionals = false;
		bool reducedGroupOptions = false;
		bool normalGroupOptions = false;
		bool hidden = false;
	};
	struct ValidLink {
		std::set<const detail::ValidArguments*> links;
		const detail::ValidArguments* owner = nullptr;
	};
	struct ValidInformation : public detail::ValidLink {
		const detail::Information* info = nullptr;
	};
	struct ValidOption : public detail::ValidLink {
		const detail::Option* option = nullptr;
		size_t minimumEffective = 0;
		size_t minimumActual = 0;
		size_t maximum = 0;
		bool payload = false;
		bool hidden = false;
	};
	struct ValidConfig : public detail::ValidArguments {
		const detail::Config* burned = nullptr;
		std::list<detail::ValidInformation> infos;
		std::map<std::wstring, detail::ValidOption> options;
		std::map<wchar_t, detail::ValidOption*> abbreviations;
		const detail::SpecialEntry* help = nullptr;
		const detail::SpecialEntry* version = nullptr;
	};
	using RefTarget = std::variant<detail::ValidOption*, detail::ValidInformation*, detail::ValidArguments*>;
	struct ValidationState {
		std::set<size_t> optionIds;
		detail::ValidConfig& config;
		std::map<size_t, std::vector<detail::RefTarget>> refs;
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
	inline bool CheckUsage(const detail::ValidLink* link, const detail::ValidArguments* test) {
		/* check if the test, or any of the test's parents, is linked to the link */
		for (const auto& link : link->links) {
			if ((link->depth < test->depth) ? detail::CheckParent(link, test) : detail::CheckParent(test, link))
				return true;
		}
		return false;
	}

	inline constexpr void ValidateDescription(detail::ValidConfig& state, const detail::Description& description) {
		if (description.description.normal.empty() && !description.description.reduced.empty())
			throw arger::ConfigException{ L"Reduced description requires normal description as well." };
		if (!description.description.reduced.empty() && (state.help == nullptr || !state.help->reducible))
			throw arger::ConfigException{ L"Reduced description requires reduced help to be possible." };
	}
	inline constexpr void ValidateType(detail::ValidConfig& state, const arger::Type& type) {
		if (!std::holds_alternative<arger::Enum>(type))
			return;
		const arger::Enum& list = std::get<arger::Enum>(type);
		if (list.empty())
			throw arger::ConfigException{ L"Enum must not be empty." };
		for (const auto& entry : list) {
			if (entry.normal.empty() && !entry.reduced.empty())
				throw arger::ConfigException{ L"Reduced description requires normal description as well." };
			if (!entry.reduced.empty() && (state.help == nullptr || !state.help->reducible))
				throw arger::ConfigException{ L"Reduced description requires reduced help to be possible." };
		}
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
	inline void ValidateLink(const detail::ValidArguments& args, detail::ValidLink* link) {
		if (!detail::CheckParent(link->owner, &args))
			throw arger::ConfigException{ L"Group cannot be linked to objects from another group." };
		link->links.insert(&args);
	}

	inline void ValidateInformation(detail::ValidationState& state, const detail::Arguments& arguments, detail::ValidArguments* owner) {
		/* iterate over the information, validate them, and register them */
		for (const auto& info : arguments.information) {
			if (info.name.empty() || info.text.empty())
				throw arger::ConfigException{ L"Information name and description must not be empty." };
			if (!info.reduced.empty() && (state.config.help == nullptr || !state.config.help->reducible))
				throw arger::ConfigException{ L"Reduced information requires reduced help to be possible." };

			/* add the entry to the list and set it up */
			detail::ValidInformation& entry = state.config.infos.emplace_back();
			entry.owner = owner;
			entry.info = &info;

			/* register the reference-ids */
			for (size_t id : info.links)
				state.refs[id].emplace_back(&entry);
		}
	}
	inline void ValidateEndpoint(detail::ValidationState& state, detail::ValidArguments& entry, const detail::Arguments* args, const detail::Endpoint* endpoint, bool hidden) {
		const std::vector<detail::Positional>& positionals = (args == nullptr ? endpoint->positionals : args->positionals);
		std::optional<size_t> minimum = (args == nullptr ? endpoint->require : args->require).minimum;
		std::optional<size_t> maximum = (args == nullptr ? endpoint->require : args->require).maximum;

		/* setup the new endpoint (all-children is ignored for end-points as they do not have children) */
		detail::ValidEndpoint& next = entry.endpoints.emplace_back();
		next.positionals = &positionals;
		next.constraints = (args == nullptr ? &endpoint->constraints : nullptr);
		next.description = (args == nullptr ? endpoint : nullptr);
		next.id = (args == nullptr ? endpoint->id : args->endpointId);
		next.hidden = (hidden || (endpoint != nullptr && endpoint->hidden));

		/* validate the description */
		if (next.description != nullptr)
			detail::ValidateDescription(state.config, *next.description);

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
			detail::ValidateType(state.config, positionals[i].type);

			/* validate the default-value */
			if (!positionals[i].defValue.has_value())
				continue;
			detail::ValidateDefValue(positionals[i].type, *positionals[i].defValue);
			if (i < next.minimumEffective)
				throw arger::ConfigException{ L"All positionals up to the minimum must be defaulted once one is defaulted." };

			/* validat ethe description */
			detail::ValidateDescription(state.config, positionals[i]);
		}
	}
	inline void ValidateOption(detail::ValidationState& state, const detail::Option& option, const detail::ValidArguments* owner, bool hidden) {
		if (option.name.size() <= 1)
			throw arger::ConfigException{ L"Option name must at least be two characters long." };
		if (option.name.starts_with(L"-"))
			throw arger::ConfigException{ L"Option name must not start with a hypen." };

		/* check if the name is unique */
		if (state.config.options.contains(option.name))
			throw arger::ConfigException{ L"Option names must be unique." };

		/* setup the new entry */
		detail::ValidOption& entry = state.config.options[option.name];
		entry.option = &option;
		entry.payload = !option.payload.name.empty();
		entry.owner = owner;
		entry.hidden = (hidden || option.hidden);

		/* register the reference-ids */
		for (size_t id : option.links)
			state.refs[id].emplace_back(&entry);

		/* check if the abbreviation is unique */
		if (option.abbreviation != 0) {
			if (state.config.abbreviations.contains(option.abbreviation))
				throw arger::ConfigException{ L"Option abbreviations must be unique." };
			state.config.abbreviations[option.abbreviation] = &entry;
		}

		/* check if the id is unique */
		if (state.optionIds.contains(option.id))
			throw arger::ConfigException{ L"Option ids must be unique." };
		state.optionIds.insert(option.id);

		/* check if the name or abbreviation clashes with the help/version entries (for options only necessary for programs) */
		if (!state.config.burned->program.empty())
			detail::ValidateSpecialEntry(state.config, option.name, option.abbreviation);

		/* validate the description */
		detail::ValidateDescription(state.config, option);

		/* validate the payload */
		if (entry.payload)
			detail::ValidateType(state.config, option.payload.type);
		else if (option.require.minimum.has_value() || option.require.maximum.has_value())
			throw arger::ConfigException{ L"Flags cannot have requirements defined." };
		else if (!option.payload.defValue.empty())
			throw arger::ConfigException{ L"Default values are not allowed for flags without payload." };

		/* configure the minimum */
		entry.minimumActual = option.require.minimum.value_or(0);
		entry.minimumEffective = (option.payload.defValue.empty() ? entry.minimumActual : 0);

		/* validate and configure the maximum */
		if (!option.require.maximum.has_value())
			entry.maximum = std::max<size_t>(entry.minimumActual, 1);
		else if (*option.require.maximum < entry.minimumActual)
			entry.maximum = 0;
		else
			entry.maximum = *option.require.maximum;

		/* validate the default-values */
		if (entry.payload && !option.payload.defValue.empty()) {
			if (option.payload.defValue.size() < entry.minimumActual)
				throw arger::ConfigException{ L"Default values for option must not violate its own minimum requirements." };
			if (entry.maximum > 0 && option.payload.defValue.size() > entry.maximum)
				throw arger::ConfigException{ L"Default values for option must not violate its own maximum requirements." };
			for (const auto& value : option.payload.defValue)
				detail::ValidateDefValue(option.payload.type, value);
		}
	}
	inline void ValidateArguments(detail::ValidationState& state, const detail::Arguments& arguments, const detail::Group* group, detail::ValidArguments& entry, detail::ValidArguments* super, bool hidden, bool reducedGroupOptions, bool normalGroupOptions) {
		/* populate the entry */
		entry.constraints = &arguments.constraints;
		entry.nestedPositionals = !arguments.positionals.empty();
		entry.super = super;
		entry.depth = (super == nullptr ? 0 : super->depth + 1);
		entry.group = group;
		entry.args = &arguments;
		entry.hidden = (hidden || (group != nullptr && group->hidden));
		entry.reducedGroupOptions = arguments.groupReduced.value_or(reducedGroupOptions);
		entry.normalGroupOptions = arguments.groupNormal.value_or(normalGroupOptions);

		/* check if the reduced grouping is valid */
		if (arguments.groupReduced.has_value() && (state.config.help == nullptr || !state.config.help->reducible))
			throw arger::ConfigException{ L"Grouping options for reduced view requires reduced help to be possible." };

		/* register the reference-ids */
		for (size_t id : arguments.links)
			state.refs[id].emplace_back(&entry);

		/* validate and configure the group name */
		if (arguments.groups.name.empty())
			entry.groupName = L"mode";
		else
			entry.groupName = str::View{ arguments.groups.name }.lower();

		/* validate the description, information, and options */
		detail::ValidateDescription(state.config, arguments);
		detail::ValidateInformation(state, arguments, &entry);
		for (const auto& option : arguments.options)
			detail::ValidateOption(state, option, &entry, entry.hidden);

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
				if (state.config.burned->program.empty())
					detail::ValidateSpecialEntry(state.config, sub.name, sub.abbreviation);

				/* validate the entry itself and update the nesting flags */
				detail::ValidateArguments(state, sub, &sub, next, &entry, entry.hidden, entry.reducedGroupOptions, entry.normalGroupOptions);
				entry.nestedPositionals = (entry.nestedPositionals || next.nestedPositionals);
			}
			return;
		}

		/* check if an implicit endpoint needs to be added */
		if (arguments.endpoints.empty())
			detail::ValidateEndpoint(state, entry, &arguments, nullptr, entry.hidden);

		/* validate all explicit endpoints */
		else {
			if (!arguments.positionals.empty() || arguments.require.minimum.has_value() || arguments.require.maximum.has_value())
				throw arger::ConfigException{ L"Implicit and explicit endpoints cannot be used in conjunction." };
			for (size_t i = 0; i < arguments.endpoints.size(); ++i)
				detail::ValidateEndpoint(state, entry, nullptr, &arguments.endpoints[i], entry.hidden);
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
	inline void ValidateLinkEntries(detail::ValidationState& state, const detail::ValidArguments& arguments) {
		/* link all groups to information/options */
		for (size_t link : arguments.args->links) {
			auto it = state.refs.find(link);
			if (it == state.refs.end())
				throw arger::ConfigException{ L"Links cannot be created to unlinked objects." };

			/* iterate over the objects and find all information/options and validate and link them */
			for (const auto& ref : it->second) {
				/* check if its an option and the link is valid */
				if (std::holds_alternative<detail::ValidOption*>(ref))
					detail::ValidateLink(arguments, std::get<detail::ValidOption*>(ref));

				/* check if its an information and the link is valid */
				else if (std::holds_alternative<detail::ValidInformation*>(ref))
					detail::ValidateLink(arguments, std::get<detail::ValidInformation*>(ref));
			}
		}

		/* validate all children */
		for (const auto& [_, child] : arguments.sub)
			detail::ValidateLinkEntries(state, child);
	}
	inline void ValidateLinkEntry(detail::ValidationState& state, detail::ValidLink* link, const std::set<size_t>& ids) {
		/* iterate over the links and try to create them */
		for (size_t id : ids) {
			auto it = state.refs.find(id);
			if (it == state.refs.end())
				throw arger::ConfigException{ L"Links cannot be created to unlinked objects." };

			/* iterate over the ref-entires and look for groups to link with */
			for (const auto& ref : it->second) {
				if (std::holds_alternative<detail::ValidArguments*>(ref))
					detail::ValidateLink(*std::get<detail::ValidArguments*>(ref), link);
			}
		}

		/* check if the object has no links and add its owner as single link */
		if (link->links.empty())
			link->links.insert(link->owner);
	}
	inline void ValidateConfig(const arger::Config& config, detail::ValidConfig& out) {
		/* burn the config and reset the state (validated state must not
		*	outlive original config and config must not be modified inbetween) */
		out.burned = &detail::ConfigBurner::GetBurned(config);
		out.options.clear();
		out.abbreviations.clear();
		out.help = nullptr;
		out.version = nullptr;

		/* setup the the validation-state */
		detail::ValidationState state{ .config = out };

		/* validate the special-purpose entries attributes */
		if (!out.burned->special.help.name.empty()) {
			if (out.burned->special.help.name.size() <= 1)
				throw arger::ConfigException{ L"Help entry name must at least be two characters long." };
			if (out.burned->special.version.name == out.burned->special.help.name)
				throw arger::ConfigException{ L"Help entry and version entry cannot have the same name." };
			if (out.burned->special.help.reducible && out.burned->special.help.abbreviation == 0)
				throw arger::ConfigException{ L"Reducible help entry requires a defined abbreviarion." };
			if (!out.burned->special.version.name.empty()) {
				if (out.burned->special.help.abbreviation != 0 && out.burned->special.help.abbreviation == out.burned->special.version.abbreviation)
					throw arger::ConfigException{ L"Help entry and version entry cannot have the same abbreviation." };
			}
			out.help = &out.burned->special.help;
			detail::ValidateDescription(out, out.burned->special.help);
		}
		if (!out.burned->special.version.name.empty()) {
			if (out.burned->special.version.name.size() <= 1)
				throw arger::ConfigException{ L"Version entry name must at least be two characters long." };
			if (out.burned->version.empty())
				throw arger::ConfigException{ L"Version string must be set when using a version entry." };
			out.version = &out.burned->special.version;
			detail::ValidateDescription(out, out.burned->special.version);
		}

		/* validate the root arguments */
		detail::ValidateArguments(state, *out.burned, nullptr, state.config, nullptr, false, false, true);

		/* link all groups to information/options */
		detail::ValidateLinkEntries(state, state.config);

		/* link all options and information to groups and finalize them */
		for (auto& [_, option] : out.options)
			detail::ValidateLinkEntry(state, &option, option.option->links);
		for (auto& info : out.infos)
			detail::ValidateLinkEntry(state, &info, info.info->links);
	}
}
