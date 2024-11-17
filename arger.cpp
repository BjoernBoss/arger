/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#include "arger.h"

template <class Type>
static std::wstring ParseProgramName(const Type* str, bool defOnError) {
	std::basic_string_view<Type> view{ str };

	size_t begin = view.size();
	while (begin > 0 && view[begin - 1] != L'/' && view[begin - 1] != L'\\')
		--begin;

	if (begin == view.size())
		return (defOnError ? L"%unknown%" : L"");
	return str::wd::To(view.substr(begin));
}


arger::Arguments::Arguments(std::wstring version, std::wstring groupName) {
	pVersion = version;
	pGroupName = str::View{ groupName }.lower();
}

void arger::Arguments::fAddHelpNewLine(bool emptyLine, HelpState& s) const {
	if (s.buffer.empty() || s.buffer.back() != L'\n')
		s.buffer.push_back(L'\n');
	if (emptyLine)
		s.buffer.push_back(L'\n');
	s.pos = 0;
}
void arger::Arguments::fAddHelpToken(const std::wstring& add, HelpState& s) const {
	if (s.pos > 0 && s.pos + add.size() > Arguments::NumCharsHelp) {
		s.buffer.push_back(L'\n');
		s.pos = 0;
	}

	s.buffer.append(add);
	s.pos += add.size();
}
void arger::Arguments::fAddHelpSpacedToken(const std::wstring& add, HelpState& s) const {
	if (s.pos > 0) {
		if (s.pos + 1 + add.size() > Arguments::NumCharsHelp) {
			s.buffer.push_back(L'\n');
			s.pos = 0;
		}
		else {
			s.buffer.push_back(L' ');
			++s.pos;
		}
	}
	fAddHelpToken(add, s);
}
void arger::Arguments::fAddHelpString(const std::wstring& add, HelpState& s, size_t offset, size_t indentAutoBreaks) const {
	std::wstring tokenWhite, tokenPrint;
	bool isWhitespace = true;

	/* ensure the initial indentation is valid */
	if (offset > 0) {
		if (s.pos >= offset) {
			s.buffer.push_back(L'\n');
			s.pos = 0;
		}
		s.buffer.append(offset - s.pos, L' ');
		s.pos = offset;
		offset += indentAutoBreaks;
	}

	/* iterate over the string and collect all ranges of whitespace, followed by printable characters, and
	*	flush them out to the output whenever the line overflows or a newline/new printable-token is encountered */
	for (size_t i = 0; i <= add.size(); ++i) {
		if (i < add.size() && !std::iswspace(add[i])) {
			isWhitespace = false;
			tokenPrint.push_back(add[i]);
			continue;
		}

		/* check if the last whitespace and printable token need to be flushed (only if printable buffer is
		*	not empty and the next character is either the end of the string or a new whitespace character) */
		if (!isWhitespace) {
			/* check if the whitespace token should be replaced with a line-break, if the printable character
			*	does not start at the beginning of the line but exceeds the line limit, and otherwise flush
			*	the whitespace (this ensures that user-placed whitespace after a new-line will be respected) */
			if ((s.pos > offset || tokenWhite.size() > 0) && s.pos + tokenWhite.size() + tokenPrint.size() > Arguments::NumCharsHelp)
				s.buffer.append(1, L'\n').append(s.pos = offset, L' ');
			else {
				s.buffer.append(tokenWhite);
				s.pos += tokenWhite.size();
			}

			/* flush the last printable token to the buffer */
			s.buffer.append(tokenPrint);
			s.pos += tokenPrint.size();
			tokenWhite.clear();
			tokenPrint.clear();
		}
		if (i >= add.size())
			break;

		/* check if the new token is a linebreak and insert it and otherwise add the whitespace to the current token */
		isWhitespace = true;
		if (add[i] == L'\n')
			s.buffer.append(1, L'\n').append(s.pos = offset, L' ');
		else
			tokenWhite.push_back(add[i]);
	}
}

void arger::Arguments::fBuildHelpUsage(const GroupEntry* current, const std::wstring& program, HelpState& s) const {
	/* add the example-usage descriptive line */
	fAddHelpNewLine(true, s);
	fAddHelpToken(L"Usage: ", s);
	fAddHelpToken(program, s);

	/* add the general group token, or explicit group selected */
	if (!pNullGroup)
		fAddHelpSpacedToken((current == 0 ? pGroupName : current->name), s);

	/* add all required options (must all consume a payload, as flags are never required) */
	bool hasOptionals = false;
	for (const auto& opt : pOptional) {
		/* check if the entry can be skipped for this group */
		if (!opt.second.users.empty() && current != 0 && opt.second.users.count(current->name) == 0)
			continue;
		if (opt.second.minimum == 0) {
			hasOptionals = true;
			continue;
		}
		fAddHelpSpacedToken(str::wd::Build(L"--", opt.second.name, L"=<", opt.second.payload, L">"), s);
	}
	if (hasOptionals)
		fAddHelpSpacedToken(L"[options...]", s);

	/* check if a group has been selected and add the parameter of it */
	if (current != 0) for (size_t i = 0; i < current->positional.size(); ++i) {
		std::wstring token = current->positional[i].name;

		if (i + 1 >= current->positional.size() && (current->maximum == 0 || i + 1 < current->maximum))
			token += L"...";
		if (i >= current->minimum)
			token = L"[" + token + L"]";
		fAddHelpSpacedToken(token, s);
	}

	/* add the hint to the parameters of the groups */
	else for (const auto& group : pGroups) {
		if (group.second.positional.empty())
			continue;
		fAddHelpSpacedToken(L"[params...]", s);
		break;
	}
}
void arger::Arguments::fBuildHelpAddHelpContent(const std::vector<detail::Help>& content, HelpState& s) const {
	/* iterate over the help-content and append it to the help-buffer */
	for (size_t i = 0; i < content.size(); ++i) {
		fAddHelpNewLine(true, s);
		fAddHelpString(content[i].name + L" ", s);
		fAddHelpString(content[i].description, s, Arguments::NumCharsHelpLeft);
	}
}
void arger::Arguments::fBuildHelpAddOptionals(bool required, const GroupEntry* current, HelpState& s) const {
	/* iterate over the optionals and add the corresponding type */
	auto it = pOptional.begin();
	while (it != pOptional.end()) {
		if ((it->second.minimum > 0) != required) {
			++it;
			continue;
		}

		/* check if the argument is excluded by the current selected group, based on the user-requirements */
		if (!it->second.users.empty() && current != 0 && it->second.users.count(current->name) == 0) {
			++it;
			continue;
		}
		fAddHelpNewLine(false, s);

		/* construct the left help string */
		std::wstring temp = L"  ";
		if (it->second.abbreviation != L'\0')
			temp.append(1, L'-').append(1, it->second.abbreviation).append(L", ");
		temp.append(L"--").append(it->second.name);
		if (!it->second.payload.empty())
			temp.append(L"=<").append(it->second.payload).append(1, L'>').append(fBuildHelpArgValueString(it->second.type));
		fAddHelpString(temp, s);

		/* construct the description text (add the users, if the optional argument is not a general purpose argument) */
		temp = it->second.description;
		if (!pNullGroup && !it->second.users.empty() && current == 0) {
			temp += L" (Used for: ";
			size_t index = 0;
			for (const auto& grp : it->second.users)
				temp.append(index++ > 0 ? L"|" : L"").append(grp);
			temp.append(1, L')');
		}

		/* add the custom usage-limits and write the result out */
		if (it->second.minimum != 1 || it->second.maximum != 1)
			temp.append(fBuildHelpLimitString(it->second.minimum, it->second.maximum > 1 ? it->second.maximum : 0));
		fAddHelpString(temp, s, Arguments::NumCharsHelpLeft, 1);

		/* expand all enum descriptions */
		fBuildHelpAddEnumDescription(it->second.type, s);
		++it;
	}
}
void arger::Arguments::fBuildHelpAddEnumDescription(const arger::Type& type, HelpState& s) const {
	/* check if this is an enum to be added */
	if (!std::holds_alternative<arger::Enum>(type))
		return;

	/* add the separate keys */
	for (const auto& val : std::get<arger::Enum>(type)) {
		fAddHelpNewLine(false, s);
		fAddHelpString(str::wd::Build(L"- [", val.first, L"]: ", val.second), s, Arguments::NumCharsHelpLeft, 1);
	}
}
const wchar_t* arger::Arguments::fBuildHelpArgValueString(const arger::Type& type) const {
	if (std::holds_alternative<arger::Enum>(type))
		return L" [enum]";
	arger::Primitive actual = std::get<arger::Primitive>(type);
	if (actual == arger::Primitive::boolean)
		return L" [bool]";
	if (actual == arger::Primitive::unum)
		return L" [uint]";
	if (actual == arger::Primitive::inum)
		return L" [int]";
	if (actual == arger::Primitive::real)
		return L" [real]";
	return L"";
}
std::wstring arger::Arguments::fBuildHelpLimitString(size_t minimum, size_t maximum) const {
	if (minimum == 0 && maximum == 0)
		return L"";
	if (minimum > 0 && maximum > 0) {
		if (minimum == maximum)
			return str::wd::Build(L" [", minimum, L"x]");
		return str::wd::Build(L" [", minimum, L" <= _ <= ", maximum, L"]");
	}
	if (minimum > 0)
		return str::wd::Build(L" [>= ", minimum, L"]");
	return str::wd::Build(L" [<= ", maximum, L"]");
}
std::wstring arger::Arguments::fBuildHelpString(const GroupEntry* current, const std::wstring& program) const {
	HelpState out;

	/* add the initial version line and program description */
	fAddHelpToken(fBuildVersionString(program), out);
	if (!pDescription.empty()) {
		fAddHelpNewLine(true, out);
		fAddHelpString(pDescription, out, 4);
	}

	/* add the example-usage descriptive line */
	fBuildHelpUsage(current, program, out);

	/* add the group description */
	if (!pNullGroup && current != 0 && !current->description.empty()) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Description: ", out);
		fAddHelpString(current->description, out, 4);
	}

	/* check if a group was selected and add its positional agruments */
	if (current != 0 && !current->positional.empty()) {
		fAddHelpNewLine(true, out);
		if (pNullGroup)
			fAddHelpString(L"Positional Arguments: ", out);
		else
			fAddHelpString(str::wd::Build(L"Positional Arguments for ", pGroupName, " [", current->name, "]: "), out);

		/* add the positional arguments descriptions (will automatically be sorted by position) */
		for (size_t i = 0; i < current->positional.size(); ++i) {
			fAddHelpNewLine(false, out);
			fAddHelpString(str::wd::Build("  ", current->positional[i].name, fBuildHelpArgValueString(current->positional[i].type), L" "), out);
			std::wstring temp = current->positional[i].description;
			if (i + 1 >= current->positional.size() && i + 1 < current->maximum)
				temp.append(fBuildHelpLimitString(std::max<intptr_t>(current->minimum - intptr_t(i), 0), current->maximum - i));
			fAddHelpString(temp, out, Arguments::NumCharsHelpLeft, 1);
			fBuildHelpAddEnumDescription(current->positional[i].type, out);
		}
	}

	/* add the description of all groups */
	else {
		fAddHelpNewLine(true, out);
		for (const auto& group : pGroups) {
			fAddHelpNewLine(false, out);
			fAddHelpString(str::wd::Build(L"  ", group.second.name), out);
			fAddHelpString(group.second.description, out, Arguments::NumCharsHelpLeft, 1);
		}
	}

	/* check if there are optional/required arguments */
	bool optArgs = false, reqArgs = false;
	for (const auto& entry : pOptional) {
		if (entry.second.users.empty() || current == 0 || entry.second.users.count(current->name) > 0)
			(entry.second.minimum > 0 ? reqArgs : optArgs) = true;
	}

	/* add the required argument descriptions (will automatically be sorted lexicographically) */
	if (reqArgs) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Required arguments:", out);
		fBuildHelpAddOptionals(true, current, out);
	}

	/* add the optional argument descriptions (will automatically be sorted lexicographically) */
	if (optArgs) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Optional arguments:", out);
		fBuildHelpAddOptionals(false, current, out);
	}

	/* add all of the global help descriptions */
	fBuildHelpAddHelpContent(pHelpContent, out);

	/* add the help descriptions of the selected group */
	if (current != 0)
		fBuildHelpAddHelpContent(current->helpContent, out);
	return out.buffer;
}
std::wstring arger::Arguments::fBuildVersionString(const std::wstring& program) const {
	return str::wd::Build(program, L" Version [", pVersion, L"]");
}

void arger::Arguments::fParseValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const {
	/* check if an enum was expected */
	if (std::holds_alternative<arger::Enum>(type)) {
		const arger::Enum& expected = std::get<arger::Enum>(type);
		if (expected.count(value.str()) != 0)
			return;
		throw arger::ParsingException(L"Invalid enum for argument [", name, L"] encountered.");
	}

	/* validate the expected type and found value */
	switch (std::get<arger::Primitive>(type)) {
	case arger::Primitive::inum: {
		auto [num, len, res] = str::ParseNum<int64_t>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw arger::ParsingException(L"Invalid signed integer for argument [", name, L"] encountered.");
		value = arger::Value{ num };
		break;
	}
	case arger::Primitive::unum: {
		auto [num, len, res] = str::ParseNum<uint64_t>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw arger::ParsingException(L"Invalid unsigned integer for argument [", name, L"] encountered.");
		value = arger::Value{ num };
		break;
	}
	case arger::Primitive::real: {
		auto [num, len, res] = str::ParseNum<double>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw arger::ParsingException(L"Invalid real for argument [", name, L"] encountered.");
		value = arger::Value{ num };
		break;
	}
	case arger::Primitive::boolean: {
		if (str::View{ value.str() }.icompare(L"true") || value.str() == L"1") {
			value = arger::Value{ true };
			break;
		}
		if (str::View{ value.str() }.icompare(L"false") || value.str() == L"0") {
			value = arger::Value{ false };
			break;
		}
		throw arger::ParsingException(L"Invalid boolean for argument [", name, L"] encountered.");
	}
	case arger::Primitive::any:
	default:
		break;
	}
}
void arger::Arguments::fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s) {
	bool payloadUsed = false;

	/* iterate over the list of optional abbreviations/single full-name and process them */
	for (size_t i = 0; i < arg.size(); ++i) {
		OptionalEntry* entry = 0;

		/* resolve the optional-argument entry, depending on it being a short abbreviation, or a full name */
		if (fullName) {
			auto it = pOptional.find(arg);
			if (it == pOptional.end())
				throw arger::ParsingException(L"Unknown optional argument [", arg, L"] encountered.");
			entry = &it->second;
			i = arg.size();
		}
		else {
			auto it = pAbbreviations.find(arg[i]);
			if (it == pAbbreviations.end())
				throw arger::ParsingException(L"Unknown optional argument-abbreviation [", arg[i], L"] encountered.");
			entry = it->second;
		}

		/* check if this is a flag and mark it as seen and check if its a special purpose argument */
		if (entry->payload.empty()) {
			s.parsed.pFlags.insert(entry->name);
			if (entry->helpFlag)
				s.printHelp = true;
			else if (entry->versionFlag)
				s.printVersion = true;
			continue;
		}

		/* check if the payload has already been consumed */
		if (payloadUsed || (payload.empty() && s.index >= s.args.size()))
			throw arger::ParsingException(L"Value [", entry->payload, L"] missing for optional argument [", entry->name, L"].");
		payloadUsed = true;

		/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
		auto it = s.parsed.pOptions.find(entry->name);
		if (it == s.parsed.pOptions.end())
			it = s.parsed.pOptions.insert({ entry->name, {} }).first;
		it->second.emplace_back(arger::Value{ payload.empty() ? s.args[s.index++] : payload });
	}

	/* check if a payload was supplied but not consumed */
	if (!payload.empty() && !payloadUsed)
		throw arger::ParsingException(L"Value [", payload, L"] not used by optional arguments.");
}
void arger::Arguments::fVerifyPositional(ArgState& s) {
	/* check if no group has been selected (because of no arguments having been passed to the program) */
	if (s.current == 0)
		throw arger::ParsingException(str::View{ pGroupName }.title(), L" missing.");

	/* validate the requirements for the positional arguments and parse their values */
	for (size_t i = 0; i < s.parsed.pPositional.size(); ++i) {
		/* check if the argument is out of range */
		if (s.current->positional.empty() || (s.current->maximum > 0 && i >= s.current->maximum)) {
			if (s.current->name.empty())
				throw arger::ParsingException(L"Unrecognized argument [", s.parsed.pPositional[i].str(), L"] encountered.");
			throw arger::ParsingException(L"Unrecognized argument [", s.parsed.pPositional[i].str(), L"] encountered for ", pGroupName, L" [", s.current->name, L"].");
		}

		/* parse the argument */
		size_t index = std::min<size_t>(i, s.current->positional.size() - 1);
		fParseValue(s.current->positional[index].name, s.parsed.pPositional[i], s.current->positional[index].type);
	}

	/* check if the minimum required number of parameters has not been reached (maximum not necessary to be checked, as it will be checked implicitly by the verification-loop) */
	if (s.parsed.pPositional.size() < s.current->minimum) {
		size_t index = std::min<size_t>(s.current->positional.size() - 1, s.parsed.pPositional.size());
		if (s.current->name.empty())
			throw arger::ParsingException(L"Argument [", s.current->positional[index].name, L"] missing.");
		throw arger::ParsingException(L"Argument [", s.current->positional[index].name, L"] missing for ", pGroupName, L" [", s.current->name, L"].");
	}
}
void arger::Arguments::fVerifyOptional(ArgState& s) {
	/* iterate over the optional arguments and verify them */
	for (auto& opt : pOptional) {
		/* check if the current group is not a user of the optional argument (current cannot be null) */
		if (!opt.second.users.empty() && opt.second.users.count(s.current->name) == 0) {
			if ((opt.second.payload.empty() ? s.parsed.pFlags.count(opt.first) : s.parsed.pOptions.count(opt.first)) > 0)
				throw arger::ParsingException(L"Argument [", opt.second.name, L"] not meant for ", pGroupName, L" [", s.current->name, L"].");
			continue;
		}

		/* check if this is a flag, in which case nothing more needs to be checked */
		if (opt.second.payload.empty())
			continue;

		/* lookup the option */
		auto it = s.parsed.pOptions.find(opt.first);
		size_t count = (it == s.parsed.pOptions.end() ? 0 : it->second.size());

		/* check if the optional-argument has been found */
		if (opt.second.minimum > count)
			throw arger::ParsingException(L"Argument [", opt.second.name, L"] missing.");

		/* check if too many instances were found */
		if (opt.second.maximum > 0 && count > opt.second.maximum)
			throw arger::ParsingException(L"Argument [", opt.second.name, L"] can at most be specified ", opt.second.maximum, " times.");

		/* verify the values themselves */
		for (size_t i = 0; i < count; ++i)
			fParseValue(opt.second.name, it->second[i], opt.second.type);
	}
}
arger::Parsed arger::Arguments::fParseArgs(std::vector<std::wstring> args) {
	if (pGroups.empty() || pVersion.empty())
		throw arger::ParsingException(L"Misconfigured arguments-parser.");

	/* sanitize the constraints */
	for (auto& entry : pOptional) {
		auto it = entry.second.users.begin();
		while (it != entry.second.users.end()) {
			if (pNullGroup || pGroups.find(*it) == pGroups.end())
				it = entry.second.users.erase(it);
			else
				++it;
		}
	}

	/* setup the arguments-state */
	ArgState state{ std::move(args), 0, arger::Parsed{}, L"", 1, false, false, false };

	/* parse the program name */
	if (state.args.empty())
		throw arger::ParsingException(L"Malformed arguments with no program-name.");
	state.program = ParseProgramName(state.args[0].c_str(), false);
	if (state.program.empty())
		throw arger::ParsingException(L"Malformed program path argument [", state.args[0], L"] encountered.");

	/* check if groups are not used, in which case the null group will be implicitly selected */
	if (pNullGroup)
		state.current = &pGroups.begin()->second;

	/* iterate over the arguments and parse them based on the definitions */
	while (state.index < state.args.size()) {
		const std::wstring& next = state.args[state.index++];

		/* check if its an optional argument or positional-lock */
		if (!state.positionalLocked && !next.empty() && next[0] == L'-') {
			if (next == L"--") {
				state.positionalLocked = true;
				continue;
			}

			/* check if a payload is baked into the string */
			std::wstring payload;
			size_t end = next.size();
			for (size_t i = 1; i < next.size(); ++i) {
				if (next[i] != L'=')
					continue;

				/* check if the payload is well-formed (--name=payload) and split it off */
				if (i + 1 >= next.size())
					throw arger::ParsingException(L"Malformed payload assigned to optional argument [", next, L"].");
				payload = next.substr(i + 1);
				end = i;
				break;
			}

			/* parse the single long or multiple short arguments */
			size_t hypenCount = (next.size() > 2 && next[1] == L'-' ? 2 : 1);
			fParseOptional(next.substr(hypenCount, end - hypenCount), payload, (hypenCount == 2), state);
			continue;
		}

		/* check if this is the group-selector */
		if (state.current == 0) {
			/* find the group with the matching argument-name */
			auto it = pGroups.find(next);

			/* check if a group has been found */
			if (it == pGroups.end())
				throw arger::ParsingException(L"Unknown ", pGroupName, L" [", next, L"] encountered.");
			state.current = &it->second;
			continue;
		}

		/* add the argument to the list of positional arguments (dont perform any validations or limit checks for now) */
		state.parsed.pPositional.emplace_back(arger::Value{ next });
	}

	/* check if the version or help should be printed */
	if (state.printHelp)
		throw arger::PrintMessage(fBuildHelpString(state.current, state.program));
	if (state.printVersion)
		throw arger::PrintMessage(fBuildVersionString(state.program));

	/* verify the positional arguments */
	fVerifyPositional(state);

	/* verify the optional arguments */
	fVerifyOptional(state);

	/* current cannot be null, as fVerifyPositional will otherwise throw, but intellisense cannot detect it */
	if (state.current == 0)
		return state.parsed;

	/* setup the selected group */
	state.parsed.pGroup = state.current->name;

	/* validate all constraints */
	for (const auto& fn : pConstraints) {
		std::wstring err = fn(state.parsed);
		if (!err.empty())
			throw arger::ParsingException(err);
	}
	for (const auto& opt : pOptional) {
		if (state.parsed.pOptions.count(opt.first) == 0)
			continue;
		for (const auto& fn : opt.second.constraints) {
			std::wstring err = fn(state.parsed);
			if (!err.empty())
				throw arger::ParsingException(err);
		}
	}
	for (const auto& fn : state.current->constraints) {
		std::wstring err = fn(state.parsed);
		if (!err.empty())
			throw arger::ParsingException(err);
	}
	return state.parsed;
}

void arger::Arguments::addGlobal(const arger::Configuration& config) {
	for (size_t i = 0; i < config.help.size(); ++i) {
		if (!config.help[i].name.empty())
			pHelpContent.push_back(config.help[i]);
	}
	for (size_t i = 0; i < config.constraints.size(); ++i)
		pConstraints.push_back(config.constraints[i]);

	if (!config.description.empty())
		pDescription = config.description;
}
void arger::Arguments::addOption(const std::wstring& name, const arger::Configuration& config) {
	/* validate the general configuration */
	if (pOptional.count(name) > 0 || name.empty())
		return;
	if (!config.payload.empty() && std::holds_alternative<arger::Enum>(config.type) && std::get<arger::Enum>(config.type).empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptionalEntry& entry = pOptional.insert({ name, OptionalEntry{} }).first->second;
	if (config.abbreviation != L'\0' && pAbbreviations.count(config.abbreviation) == 0)
		pAbbreviations.insert({ config.abbreviation, &entry });

	/* populate the new argument-entry and sanitize it further */
	entry.name = name;
	entry.description = config.description;
	entry.abbreviation = config.abbreviation;
	entry.helpFlag = config.helpFlag;
	entry.versionFlag = (!entry.helpFlag && config.versionFlag);
	if (entry.helpFlag || entry.versionFlag)
		return;
	entry.users = config.users;
	entry.constraints = config.constraints;

	/* check if the option is a flag or an option */
	if (config.payload.empty())
		return;
	entry.payload = config.payload;
	entry.type = config.type;

	/* configure the minimum and maximum constraints */
	entry.minimum = (config.minimumSet ? config.minimum : 0);
	if (config.maximumSet)
		entry.maximum = (config.maximum == 0 ? 0 : std::max<size_t>(entry.minimum, config.maximum));
	else
		entry.maximum = std::max<size_t>(entry.minimum, 1);
}
void arger::Arguments::addGroup(const std::wstring& name, const arger::Configuration& config) {
	/* check if the group has already been defined or a group is being added to no-group configuration/empty group added to groups */
	if (pGroups.count(name) > 0)
		return;
	if (pGroups.empty())
		pNullGroup = name.empty();
	else if (pNullGroup || name.empty())
		return;

	/* setup the group-entry */
	GroupEntry& entry = pGroups.insert({ name, GroupEntry() }).first->second;
	entry.name = name;
	entry.description = config.description;
	for (size_t i = 0; i < config.help.size(); ++i) {
		if (!config.help[i].name.empty())
			entry.helpContent.push_back(config.help[i]);
	}
	for (size_t i = 0; i < config.positional.size(); ++i) {
		if (config.positional[i].name.empty())
			continue;
		if (std::holds_alternative<arger::Enum>(config.positional[i].type) && std::get<arger::Enum>(config.positional[i].type).empty())
			continue;
		entry.positional.push_back(config.positional[i]);
	}
	entry.constraints = config.constraints;

	/* configure the minimum/maximum constraints */
	entry.minimum = ((config.minimumSet && !entry.positional.empty()) ? config.minimum : entry.positional.size());
	if (config.maximumSet && !entry.positional.empty())
		entry.maximum = (config.maximum == 0 ? 0 : std::max<size_t>({ entry.minimum, config.maximum, entry.positional.size() }));
	else
		entry.maximum = std::max<size_t>(entry.minimum, entry.positional.size());
}

arger::Parsed arger::Arguments::parse(int argc, const char* const* argv) {
	std::vector<std::wstring> args;

	/* convert the arguments and parse them */
	for (size_t i = 0; i < argc; ++i)
		args.push_back(str::wd::To(argv[i]));
	return fParseArgs(args);
}
arger::Parsed arger::Arguments::parse(int argc, const wchar_t* const* argv) {
	std::vector<std::wstring> args;

	/* convert the arguments and parse them */
	for (size_t i = 0; i < argc; ++i)
		args.emplace_back(argv[i]);
	return fParseArgs(args);
}

std::wstring arger::HelpHint(int argc, const char* const* argv) {
	std::wstring program = ParseProgramName(argc == 0 ? 0 : argv[0], true);
	return str::wd::Build(L"Try '", program, L" --help' for more information.");
}
std::wstring arger::HelpHint(int argc, const wchar_t* const* argv) {
	std::wstring program = ParseProgramName(argc == 0 ? 0 : argv[0], true);
	return str::wd::Build(L"Try '", program, L" --help' for more information.");
}
