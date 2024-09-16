#include "prog-args.h"

void arger::Parser::fAddHelpNewLine(bool emptyLine, HelpState& s) const {
	if (s.buffer.empty() || s.buffer.back() != L'\n')
		s.buffer.push_back(L'\n');
	if (emptyLine)
		s.buffer.push_back(L'\n');
	s.pos = 0;
}
void arger::Parser::fAddHelpToken(const std::wstring& add, HelpState& s) const {
	if (s.pos > 0 && s.pos + add.size() > Parser::NumCharsHelp) {
		s.buffer.push_back(L'\n');
		s.pos = 0;
	}

	s.buffer.append(add);
	s.pos += add.size();
}
void arger::Parser::fAddHelpSpacedToken(const std::wstring& add, HelpState& s) const {
	if (s.pos > 0) {
		if (s.pos + 1 + add.size() > Parser::NumCharsHelp) {
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
void arger::Parser::fAddHelpString(const std::wstring& add, HelpState& s, size_t offset, size_t indentAutoBreaks) const {
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
			if ((s.pos > offset || tokenWhite.size() > 0) && s.pos + tokenWhite.size() + tokenPrint.size() > Parser::NumCharsHelp)
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

void arger::Parser::fBuildHelpUsage(HelpState& s) const {
	/* add the example-usage descriptive line */
	fAddHelpNewLine(true, s);
	fAddHelpToken(L"Usage: ", s);
	fAddHelpToken(pProgram, s);

	/* add the general group token, or explicit group selected */
	if (!pNullGroup)
		fAddHelpSpacedToken((pCurrent == 0 ? pGroupName : pCurrent->name), s);

	/* add all required options (must all consume a payload, as flags are never required) */
	bool hasOptionals = false;
	for (const auto& opt : pOptional) {
		/* check if the entry can be skipped for this group */
		if (!pNullGroup && !opt.second.generalPurpose && pCurrent != 0 && opt.second.users.count(pCurrent->name) == 0)
			continue;
		if (!opt.second.required) {
			hasOptionals = true;
			continue;
		}
		fAddHelpSpacedToken(str::Build<std::wstring>(L"--", opt.second.name, L"=<", opt.second.payload, L">"), s);
	}
	if (hasOptionals)
		fAddHelpSpacedToken(L"[options...]", s);

	/* check if a group has been selected and add the parameter of it */
	if (pCurrent != 0) for (size_t i = 0; i < pCurrent->positional.size(); ++i) {
		std::wstring token = pCurrent->positional[i].name;

		if (i + 1 >= pCurrent->positional.size() && pCurrent->catchAll)
			token += L"...";
		if (i >= pCurrent->required)
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
void arger::Parser::fBuildHelpAddHelpContent(const std::vector<HelpEntry>& content, HelpState& s) const {
	/* iterate over the help-content and append it to the help-buffer */
	for (size_t i = 0; i < content.size(); ++i) {
		fAddHelpNewLine(true, s);
		fAddHelpString(content[i].name + L" ", s);
		fAddHelpString(content[i].description, s, Parser::NumCharsHelpLeft);
	}
}
void arger::Parser::fBuildHelpAddOptionals(bool required, HelpState& s) const {
	/* iterate over the optionals and add the corresponding type */
	auto it = pOptional.begin();
	while (it != pOptional.end()) {
		if (it->second.required != required) {
			++it;
			continue;
		}

		/* check if the argument is excluded by the current selected group, based on the user-requirements */
		if (!pNullGroup && !it->second.generalPurpose && pCurrent != 0 && it->second.users.count(pCurrent->name) == 0) {
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
		if (!pNullGroup && !it->second.generalPurpose && pCurrent == 0) {
			temp += L" (Used for: ";
			size_t index = 0;
			for (const auto& grp : it->second.users)
				temp.append(index++ > 0 ? L"|" : L"").append(grp);
			temp.append(1, L')');
		}
		fAddHelpString(temp, s, Parser::NumCharsHelpLeft, 1);

		/* expand all enum descriptions */
		fBuildHelpAddEnumDescription(it->second.type, s);
		++it;
	}
}
void arger::Parser::fBuildHelpAddEnumDescription(const arger::Type& type, HelpState& s) const {
	/* check if this is an enum to be added */
	if (!std::holds_alternative<arger::Enum>(type))
		return;

	/* add the separate keys */
	for (const auto& val : std::get<arger::Enum>(type)) {
		fAddHelpNewLine(false, s);
		fAddHelpString(str::Build<std::wstring>(L"- [", val.first, L"]: ", val.second), s, Parser::NumCharsHelpLeft, 1);
	}
}
const wchar_t* arger::Parser::fBuildHelpArgValueString(const arger::Type& type) const {
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
std::wstring arger::Parser::fBuildHelpString() const {
	HelpState out;

	/* add the initial version line and program description */
	fAddHelpToken(fBuildVersionString(), out);
	if (!pDescription.empty()) {
		fAddHelpNewLine(true, out);
		fAddHelpString(pDescription, out, 4);
	}

	/* add the example-usage descriptive line */
	fBuildHelpUsage(out);

	/* add the group description */
	if (!pNullGroup && pCurrent != 0 && !pCurrent->description.empty()) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Description: ", out);
		fAddHelpString(pCurrent->description, out, 4);
	}

	/* check if a group was selected and add its positional agruments */
	if (pCurrent != 0 && !pCurrent->positional.empty()) {
		fAddHelpNewLine(true, out);
		if (pNullGroup)
			fAddHelpString(L"Positional Arguments: ", out);
		else
			fAddHelpString(str::Build<std::wstring>(L"Positional Arguments for ", pGroupName, " [", pCurrent->name, "]: "), out);

		/* add the positional arguments descriptions (will automatically be sorted by position) */
		for (size_t i = 0; i < pCurrent->positional.size(); ++i) {
			fAddHelpNewLine(false, out);
			fAddHelpString(str::Build<std::wstring>("  ", pCurrent->positional[i].name, fBuildHelpArgValueString(pCurrent->positional[i].type), L" "), out);
			fAddHelpString(pCurrent->positional[i].description, out, Parser::NumCharsHelpLeft, 1);
			fBuildHelpAddEnumDescription(pCurrent->positional[i].type, out);
		}
	}

	/* add the description of all groups */
	else {
		fAddHelpNewLine(true, out);
		for (const auto& group : pGroups) {
			fAddHelpNewLine(false, out);
			fAddHelpString(str::Build<std::wstring>(L"  ", group.second.name), out);
			fAddHelpString(group.second.description, out, Parser::NumCharsHelpLeft, 1);
		}
	}

	/* check if there are optional/required arguments */
	bool optArgs = false, reqArgs = false;
	for (const auto& entry : pOptional) {
		if (pNullGroup || entry.second.generalPurpose || pCurrent == 0 || entry.second.users.count(pCurrent->name) > 0)
			(entry.second.required ? reqArgs : optArgs) = true;
	}

	/* add the required argument descriptions (will automatically be sorted lexicographically) */
	if (reqArgs) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Required arguments:", out);
		fBuildHelpAddOptionals(true, out);
	}

	/* add the optional argument descriptions (will automatically be sorted lexicographically) */
	if (optArgs) {
		fAddHelpNewLine(true, out);
		fAddHelpString(L"Optional arguments:", out);
		fBuildHelpAddOptionals(false, out);
	}

	/* add all of the global help descriptions */
	fBuildHelpAddHelpContent(pHelpContent, out);

	/* add the help descriptions of the selected group */
	if (pCurrent != 0)
		fBuildHelpAddHelpContent(pCurrent->helpContent, out);
	return out.buffer;
}
std::wstring arger::Parser::fBuildVersionString() const {
	return str::Build<std::wstring>(pProgram, L" Version [", pVersion, L"]");
}

void arger::Parser::fParseValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const {
	/* check if an enum was expected */
	if (std::holds_alternative<arger::Enum>(type)) {
		const arger::Enum& expected = std::get<arger::Enum>(type);
		if (expected.count(value.str()) != 0)
			return;
		throw fException(L"Invalid enum for argument [", name, L"] encountered.");
	}

	/* validate the expected type and found value */
	switch (std::get<arger::Primitive>(type)) {
	case arger::Primitive::inum: {
		auto [num, len, res] = str::ParseNum<int64_t>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw fException(L"Invalid signed integer for argument [", name, L"] encountered.");
		value = arger::Value{ num };
		break;
	}
	case arger::Primitive::unum: {
		auto [num, len, res] = str::ParseNum<uint64_t>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw fException(L"Invalid unsigned integer for argument [", name, L"] encountered.");
		value = arger::Value{ num };
		break;
	}
	case arger::Primitive::real: {
		auto [num, len, res] = str::ParseNum<double>(value.str(), 10, str::PrefixMode::overwrite);
		if (res != str::NumResult::valid || len != value.str().size())
			throw fException(L"Invalid real for argument [", name, L"] encountered.");
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
		throw fException(L"Invalid boolean for argument [", name, L"] encountered.");
	}
	case arger::Primitive::any:
	default:
		break;
	}
}
void arger::Parser::fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s) {
	bool payloadUsed = false;

	/* iterate over the list of optional abbreviations/single full-name and process them */
	for (size_t i = 0; i < arg.size(); ++i) {
		OptArg* entry = 0;

		/* resolve the optional-argument entry, depending on it being a short abbreviation, or a full name */
		if (fullName) {
			auto it = pOptional.find(arg);
			if (it == pOptional.end())
				throw fException(L"Unknown optional argument [", arg, L"] encountered.");
			entry = &it->second;
			i = arg.size();
		}
		else {
			auto it = pAbbreviations.find(arg[i]);
			if (it == pAbbreviations.end())
				throw fException(L"Unknown optional argument-abbreviation [", arg[i], L"] encountered.");
			entry = it->second;
		}

		/* check if this is a flag and mark it as seen and check if its a special purpose argument */
		if (entry->payload.empty()) {
			if (entry->parsed.empty())
				entry->parsed.emplace_back(arger::Value{ true });
			if (entry->helpFlag)
				s.printHelp = true;
			else if (entry->versionFlag)
				s.printVersion = true;
			continue;
		}

		/* check if the payload has already been consumed */
		if (payloadUsed || (payload.empty() && s.index >= s.args.size()))
			throw fException(L"Value [", entry->payload, L"] missing for optional argument [", entry->name, L"].");
		payloadUsed = true;

		/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
		entry->parsed.emplace_back(arger::Value{ payload.empty() ? s.args[s.index++] : payload });
	}

	/* check if a payload was supplied but not consumed */
	if (!payload.empty() && !payloadUsed)
		throw fException(L"Value [", payload, L"] not used by optional arguments.");
}
void arger::Parser::fVerifyPositional() {
	/* check if no group has been selected (because of no arguments having been passed to the program) */
	if (pCurrent == 0)
		throw fException(str::View{ pGroupName }.title(), L" missing.");

	/* validate the requirements for the positional arguments and parse their values */
	for (size_t i = 0; i < pPositional.size(); ++i) {
		/* check if the argument is out of range */
		if (pCurrent->positional.empty() || (i >= pCurrent->positional.size() && !pCurrent->catchAll)) {
			if (pCurrent->name.empty())
				throw fException(L"Unrecognized argument [", pPositional[i].str(), L"] encountered.");
			throw fException(L"Unrecognized argument [", pPositional[i].str(), L"] encountered for ", pGroupName, L" [", pCurrent->name, L"].");
		}

		/* parse the argument */
		size_t index = std::min<size_t>(i, pCurrent->positional.size() - 1);
		fParseValue(pCurrent->positional[index].name, pPositional[i], pCurrent->positional[index].type);
	}

	/* check if the minimum required number of parameters has not been reached */
	if (pPositional.size() >= pCurrent->required)
		return;
	else if (pCurrent->name.empty())
		throw fException(L"Arguments missing.");
	throw fException(L"Arguments missing for ", pGroupName, L" [", pCurrent->name, L"].");
}
void arger::Parser::fVerifyOptional() {
	/* iterate over the optional arguments and verify them */
	for (auto& opt : pOptional) {
		/* check if the current group is not a user of the optional argument (current cannot be null) */
		if (!pNullGroup && !opt.second.generalPurpose && opt.second.users.count(pCurrent->name) == 0) {
			if (!opt.second.parsed.empty())
				throw fException(L"Argument [", opt.second.name, L"] not meant for ", pGroupName, L" [", pCurrent->name, L"].");
			continue;
		}

		/* check if the optional-argument has been found */
		if (opt.second.parsed.empty()) {
			if (opt.second.required)
				throw fException(L"Required argument [", opt.second.name, L"] missing.");
			continue;
		}

		/* check if too many instances were found */
		if (opt.second.parsed.size() > 1 && !opt.second.multiple)
			throw fException(L"Argument [", opt.second.name, L"] can only be specified once.");

		/* verify the values themselves */
		for (size_t i = 0; i < opt.second.parsed.size(); ++i)
			fParseValue(opt.second.name, opt.second.parsed[i], opt.second.type);
	}
}
void arger::Parser::fParseName(const std::wstring& arg) {
	pProgram = L"%unknown%";

	size_t begin = arg.size();
	while (begin > 0 && arg[begin - 1] != L'/' && arg[begin - 1] != L'\\')
		--begin;

	/* check if a valid name has been encountered */
	if (begin == arg.size())
		throw fException(L"Malformed program path argument [", arg, L"] encountered.");
	pProgram = arg.substr(begin);
}
void arger::Parser::fParseArgs(std::vector<std::wstring> args) {
	if (pGroups.empty() || pVersion.empty())
		throw fException(L"Misconfigured arguments-parser.");

	/* reset the state of the last parsing */
	pCurrent = 0;
	pPositional.clear();
	for (auto& opt : pOptional)
		opt.second.parsed.clear();

	/* setup the arguments-state */
	ArgState state{ std::move(args), 0, false, false, false };

	/* check if groups are not used, in which case the null group will be implicitly selected */
	if (pNullGroup)
		pCurrent = &pGroups.begin()->second;

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
				if (std::isalnum(next[i]) || next[i] == '-' || next[i] == L'_')
					continue;
				if (next[i] != L'=')
					break;

				/* check if the payload is well-formed (--name=payload) and split it off */
				if (i + 1 >= next.size())
					throw fException(L"Malformed payload assigned to optional argument [", next, L"].");
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
		if (pCurrent == 0) {
			/* find the group with the matching argument-name */
			auto it = pGroups.find(next);

			/* check if a group has been found */
			if (it == pGroups.end())
				throw fException(L"Unknown ", pGroupName, L" [", next, L"] encountered.");
			pCurrent = &it->second;
			continue;
		}

		/* add the argument to the list of positional arguments (dont perform any validations or limit checks for now) */
		pPositional.emplace_back(arger::Value{ next });
	}

	/* check if the version or help should be printed */
	if (state.printHelp)
		throw arger::ArgPrintMessage{ fBuildHelpString() };
	if (state.printVersion)
		throw arger::ArgPrintMessage{ fBuildVersionString() };

	/* verify the positional arguments */
	fVerifyPositional();

	/* verify the optional arguments */
	fVerifyOptional();

	/* validate all constraints */
	for (const auto& con : pConstraints) {
		if (con.type == BindType::group && pCurrent->name != con.binding)
			continue;
		else if (con.type == BindType::argument && pOptional[con.binding].parsed.empty())
			continue;
		else if (con.type == BindType::positional && (pCurrent->name != con.binding || pPositional.size() <= con.index))
			continue;
		std::wstring err = con.fn(*this);
		if (!err.empty())
			throw arger::ArgsParsingException{ err };
	}
}

void arger::Parser::configure(std::wstring version, std::wstring desc, std::wstring groupName) {
	pVersion = version;
	pDescription = desc;
	pGroupName = str::View{ groupName }.lower();
}
void arger::Parser::addGlobalHelp(std::wstring name, std::wstring desc) {
	if (!name.empty())
		pHelpContent.emplace_back(name, desc);
}

void arger::Parser::addFlag(const std::wstring& name, wchar_t abbr, std::wstring desc, bool generalPurpose) {
	if (pOptional.count(name) > 0 || name.empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = L"";
	entry.description = desc;
	entry.generalPurpose = generalPurpose;
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = false;
	entry.versionFlag = false;
}
void arger::Parser::addOption(const std::wstring& name, wchar_t abbr, const std::wstring& payload, bool multiple, bool required, const arger::Type& type, std::wstring desc, bool generalPurpose) {
	if (pOptional.count(name) > 0 || name.empty() || payload.empty())
		return;
	if (std::holds_alternative<arger::Enum>(type) && std::get<arger::Enum>(type).empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = payload;
	entry.description = desc;
	entry.generalPurpose = generalPurpose;
	entry.abbreviation = abbr;
	entry.type = type;
	entry.multiple = multiple;
	entry.required = required;
	entry.helpFlag = false;
	entry.versionFlag = false;
}
void arger::Parser::addHelpFlag(const std::wstring& name, wchar_t abbr) {
	if (pOptional.count(name) > 0 || name.empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = L"";
	entry.generalPurpose = true;
	entry.description = L"Print this help menu.";
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = true;
}
void arger::Parser::addVersionFlag(const std::wstring& name, wchar_t abbr) {
	if (pOptional.count(name) > 0 || name.empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = L"";
	entry.generalPurpose = true;
	entry.description = L"Print the current program version.";
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = false;
	entry.versionFlag = true;
}

void arger::Parser::addGroup(const std::wstring& name, size_t required, bool lastCatchAll, std::wstring desc) {
	/* check if the group has already been defined or a group is being added to no-group configuration/empty group added to groups */
	if (pGroups.count(name) > 0)
		return;
	if (pGroups.empty())
		pNullGroup = name.empty();
	else if (pNullGroup || name.empty())
		return;

	/* populate the group-entry */
	pGroupInsert = &pGroups.insert({ name, GroupEntry() }).first->second;
	pGroupInsert->name = name;
	pGroupInsert->description = desc;
	pGroupInsert->required = required;
	pGroupInsert->catchAll = lastCatchAll;
}
void arger::Parser::addPositional(const std::wstring& name, const arger::Type& type, std::wstring desc) {
	if (name.empty() || pGroupInsert == 0)
		return;
	if (std::holds_alternative<arger::Enum>(type) && std::get<arger::Enum>(type).empty())
		return;
	PosArg& entry = pGroupInsert->positional.emplace_back();

	/* populate the new positional-entry */
	entry.name = name;
	entry.description = desc;
	entry.type = type;
}
void arger::Parser::addGroupHelp(const std::wstring& name, std::wstring desc) {
	if (!name.empty() && pGroupInsert != 0)
		pGroupInsert->helpContent.push_back({ name, desc });
}
void arger::Parser::bindSpecialFlagsOrOptions(const std::set<std::wstring>& names) {
	/* for null-groups, the concept of users does not exist, as the flags/optionals are all used by the single group */
	if (names.empty() || pGroupInsert == 0 || pNullGroup)
		return;

	/* iterate over the names and add the current group as users to them */
	for (const auto& name : names) {
		auto it = pOptional.find(name);
		if (it == pOptional.end() || it->second.generalPurpose)
			continue;
		it->second.users.insert(pGroupInsert->name);
	}
}

void arger::Parser::addConstraint(arger::Constraint fn) {
	pConstraints.emplace_back(std::move(fn), L"", 0, BindType::none);
}
void arger::Parser::addFlagOrOptionConstraint(const std::wstring& name, arger::Constraint fn) {
	if (pOptional.find(name) != pOptional.end())
		pConstraints.emplace_back(std::move(fn), name, 0, BindType::argument);
}
void arger::Parser::addGroupConstraint(const std::wstring& name, arger::Constraint fn) {
	if (pGroups.find(name) != pGroups.end())
		pConstraints.emplace_back(std::move(fn), name, 0, BindType::group);
}
void arger::Parser::addPositionalConstraint(const std::wstring& name, size_t index, arger::Constraint fn) {
	auto it = pGroups.find(name);
	if (it != pGroups.end() && index < it->second.positional.size())
		pConstraints.emplace_back(std::move(fn), name, index, BindType::positional);
}

void arger::Parser::parse(int argc, const char* const* argv) {
	if (argc == 0)
		throw fException(L"Malformed arguments with no program-name.");
	fParseName(str::View{ argv[0] }.to<std::wstring>());

	/* convert the arguments and parse them */
	std::vector<std::wstring> args;
	for (size_t i = 1; i < argc; ++i)
		args.push_back(str::View{ argv[i] }.to<std::wstring>());
	fParseArgs(args);
}
void arger::Parser::parse(int argc, const wchar_t* const* argv) {
	if (argc == 0)
		throw fException(L"Malformed arguments with no program-name.");
	fParseName(argv[0]);

	/* convert the arguments and parse them */
	std::vector<std::wstring> args;
	for (size_t i = 1; i < argc; ++i)
		args.emplace_back(argv[i]);
	fParseArgs(args);
}

bool arger::Parser::flag(const std::wstring& name) const {
	return !pOptional.at(name).parsed.empty();
}
const std::wstring& arger::Parser::group() const {
	return pCurrent->name;
}
std::wstring arger::Parser::helpHint() const {
	return str::Build<std::wstring>(L"Try '", pProgram, L" --help' for more information.");
}

size_t arger::Parser::options(const std::wstring& name) const {
	return pOptional.at(name).parsed.size();
}
std::optional<arger::Value> arger::Parser::option(const std::wstring& name, size_t index) const {
	if (index >= pOptional.at(name).parsed.size())
		return {};
	return pOptional.at(name).parsed[index];
}
size_t arger::Parser::positionals() const {
	return pPositional.size();
}
std::optional<arger::Value> arger::Parser::positional(size_t index) const {
	if (index >= pPositional.size())
		return {};
	return pPositional[index];
}
