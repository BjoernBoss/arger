#include "arguments.h"

void tools::Arguments::fAddHelpNewLine(bool emptyLine, HelpState& s) const {
	if (s.buffer.empty() || s.buffer.back() != L'\n')
		s.buffer.push_back(L'\n');
	if (emptyLine)
		s.buffer.push_back(L'\n');
	s.pos = 0;
}
void tools::Arguments::fAddHelpToken(const std::wstring& add, HelpState& s) const {
	if (s.pos > 0 && s.pos + add.size() > Arguments::NumCharsHelp) {
		s.buffer.push_back(L'\n');
		s.pos = 0;
	}

	s.buffer.append(add);
	s.pos += add.size();
}
void tools::Arguments::fAddHelpSpacedToken(const std::wstring& add, HelpState& s) const {
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
void tools::Arguments::fAddHelpString(const std::wstring& add, HelpState& s, size_t offset, size_t indentAutoBreaks) const {
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

void tools::Arguments::fBuildHelpUsage(HelpState& s) const {
	/* add the example-usage descriptive line */
	fAddHelpNewLine(true, s);
	fAddHelpToken(L"Usage: ", s);
	fAddHelpToken(pProgram, s);

	/* add the general group token, or explicit group selected */
	if (!pNullGroup)
		fAddHelpSpacedToken((pCurrent == 0 ? pGroupLower : pCurrent->name), s);

	/* add all required options (must all consume a payload, as flags are never required) */
	bool hasOptionals = false;
	for (const auto& opt : pOptional) {
		/* check if the entry can be skipped for this group */
		if (!pNullGroup && opt.second.explicitUsage && pCurrent != 0 && opt.second.users.count(pCurrent->name) == 0)
			continue;
		if (!opt.second.required) {
			hasOptionals = true;
			continue;
		}
		fAddHelpSpacedToken(fmt::Str("--", opt.second.name, "=<", opt.second.payload, ">").wide(), s);
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
void tools::Arguments::fBuildHelpAddHelpContent(const std::vector<HelpEntry>& content, HelpState& s) const {
	/* iterate over the help-content and append it to the help-buffer */
	for (size_t i = 0; i < content.size(); ++i) {
		fAddHelpNewLine(true, s);
		fAddHelpString(content[i].name + L" ", s);
		fAddHelpString(content[i].description, s, Arguments::NumCharsHelpLeft);
	}
}
void tools::Arguments::fBuildHelpAddOptionals(bool required, HelpState& s) const {
	/* iterate over the optionals and add the corresponding type */
	auto it = pOptional.begin();
	while (it != pOptional.end()) {
		if (it->second.required != required) {
			++it;
			continue;
		}

		/* check if the argument is excluded by the current selected group, based on the user-requirements */
		if (!pNullGroup && it->second.explicitUsage && pCurrent != 0 && it->second.users.count(pCurrent->name) == 0) {
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

		/* construct the description text (add the users, if the optional argument requires explicit users) */
		temp = it->second.description;
		if (!pNullGroup && it->second.explicitUsage && pCurrent == 0) {
			temp += fmt::Str(" (Used for: ").wide();
			size_t index = 0;
			for (const auto& grp : it->second.users)
				temp.append(index++ > 0 ? L"|" : L"").append(grp);
			temp.append(1, L')');
		}
		fAddHelpString(temp, s, Arguments::NumCharsHelpLeft, 1);

		/* expand all enum descriptions */
		fBuildHelpAddEnumDescription(it->second.type, s);
		++it;
	}
}
void tools::Arguments::fBuildHelpAddEnumDescription(const ArgValue& value, HelpState& s) const {
	/* check if this is an enum to be added */
	if (!std::holds_alternative<tools::ArgEnum>(value))
		return;

	/* add the separate keys */
	for (const auto& val : std::get<tools::ArgEnum>(value)) {
		fAddHelpNewLine(false, s);
		fAddHelpString(fmt::Str(L"- [", val.first, "]: ", val.second).wide(), s, Arguments::NumCharsHelpLeft, 1);
	}
}
const wchar_t* tools::Arguments::fBuildHelpArgValueString(const ArgValue& type) const {
	if (std::holds_alternative<tools::ArgEnum>(type))
		return L" [enum]";
	tools::ArgType actual = std::get<tools::ArgType>(type);
	if (actual == tools::ArgType::boolean)
		return L" [bool]";
	if (actual == tools::ArgType::unum)
		return L" [uint]";
	if (actual == tools::ArgType::inum)
		return L" [int]";
	if (actual == tools::ArgType::real)
		return L" [real]";
	return L"";
}
std::wstring tools::Arguments::fBuildHelpString() const {
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
			fAddHelpString(fmt::Str("Positional Arguments for ", pGroupLower, " [", pCurrent->name, "]: ").wide(), out);

		/* add the positional arguments descriptions (will automatically be sorted by position) */
		for (size_t i = 0; i < pCurrent->positional.size(); ++i) {
			fAddHelpNewLine(false, out);
			fAddHelpString(fmt::Str("  ", pCurrent->positional[i].name, fBuildHelpArgValueString(pCurrent->positional[i].type), L" ").wide(), out);
			fAddHelpString(pCurrent->positional[i].description, out, Arguments::NumCharsHelpLeft, 1);
			fBuildHelpAddEnumDescription(pCurrent->positional[i].type, out);
		}
	}

	/* add the description of all groups */
	else {
		fAddHelpNewLine(true, out);
		for (const auto& group : pGroups) {
			fAddHelpNewLine(false, out);
			fAddHelpString(fmt::Str("  ", group.second.name).wide(), out);
			fAddHelpString(group.second.description, out, Arguments::NumCharsHelpLeft, 1);
		}
	}

	/* check if there are optional/required arguments */
	bool optArgs = false, reqArgs = false;
	for (const auto& entry : pOptional) {
		if (pNullGroup || !entry.second.explicitUsage || pCurrent == 0 || entry.second.users.count(pCurrent->name) > 0)
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
std::wstring tools::Arguments::fBuildVersionString() const {
	return fmt::Str(pProgram, " Version [", pVersion, "]").wide();
}

std::wstring tools::Arguments::fParseValue(const std::wstring& name, json::Value& value, const ArgValue& type) const {
	/* check if an enum was expected */
	if (std::holds_alternative<tools::ArgEnum>(type)) {
		const tools::ArgEnum& expected = std::get<tools::ArgEnum>(type);
		if (expected.count(value.str()) == 0)
			return fmt::Str("Invalid value for argument [", name, "] encountered.").wide();
		return L"";
	}
	tools::ArgType expected = std::get<tools::ArgType>(type);

	/* check if any value was expected */
	if (expected == tools::ArgType::any)
		return L"";

	/* translate the type to the corresponding json type (and sanitize the casing for booleans) */
	json::Type jsonType = json::Type::boolean;
	if (expected == tools::ArgType::boolean) {
		if (value.str() == L"True")
			value = json::Str(L"true");
		else if (value.str() == L"False")
			value = json::Str(L"false");
	}
	else if (expected == tools::ArgType::inum)
		jsonType = json::Type::inumber;
	else if (expected == tools::ArgType::unum)
		jsonType = json::Type::unumber;
	else if (expected == tools::ArgType::real)
		jsonType = json::Type::real;

	/* try to parse the value */
	try {
		json::Value jsonValue = json::Deserialize(value.str());
		if ((value = jsonValue).is(jsonType))
			return L"";
	}
	catch (const json::JsonDeserializeException&) {}
	return fmt::Str("Invalid value for argument [", name, "] encountered.").wide();
}
std::wstring tools::Arguments::fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s) {
	bool payloadUsed = false;

	/* iterate over the list of optional abbreviations/single full-name and process them */
	for (size_t i = 0; i < arg.size(); ++i) {
		OptArg* entry = 0;

		/* resolve the optional-argument entry, depending on it being a short abbreviation, or a full name */
		if (fullName) {
			auto it = pOptional.find(arg);
			if (it == pOptional.end())
				return fmt::Str("Unknown optional argument [", arg, "] encountered.").wide();
			entry = &it->second;
			i = arg.size();
		}
		else {
			auto it = pAbbreviations.find(arg[i]);
			if (it == pAbbreviations.end())
				return fmt::Str("Unknown optional argument-abbreviation [", arg[i], "] encountered.").wide();
			entry = it->second;
		}

		/* check if this is a flag and mark it as seen and check if its a special purpose argument */
		if (entry->payload.empty()) {
			if (entry->parsed.empty())
				entry->parsed.emplace_back();
			if (entry->helpFlag)
				s.printHelp = true;
			else if (entry->versionFlag)
				s.printVersion = true;
			continue;
		}

		/* check if the payload has already been consumed */
		if (payloadUsed || (payload.empty() && s.index >= s.args.size()))
			return fmt::Str("Value [", entry->payload, "] missing for optional argument [", entry->name, "].").wide();
		payloadUsed = true;

		/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
		entry->parsed.emplace_back() = json::Str(payload.empty() ? s.args[s.index++] : payload);
	}

	/* check if a payload was supplied but not consumed */
	if (!payload.empty() && !payloadUsed)
		return fmt::Str("Value [", payload, "] not used by optional arguments.").wide();
	return L"";
}
std::wstring tools::Arguments::fVerifyPositional() {
	/* check if no group has been selected (because of no arguments having been passed to the program) */
	if (pCurrent == 0)
		return fmt::Str(pGroupUpper, " missing.").wide();

	/* validate the requirements for the positional arguments and parse their values */
	for (size_t i = 0; i < pPositional.size(); ++i) {
		/* check if the argument is out of range */
		if (pCurrent->positional.empty() || (i >= pCurrent->positional.size() && !pCurrent->catchAll)) {
			if (pCurrent->name.empty())
				return fmt::Str("Unrecognized argument [", pPositional[i].str(), "] encountered.").wide();
			return fmt::Str("Unrecognized argument [", pPositional[i].str(), "] encountered for ", pGroupLower, " [", pCurrent->name, "].").wide();
		}

		/* parse the argument */
		size_t index = std::min<size_t>(i, pCurrent->positional.size() - 1);
		std::wstring err = fParseValue(pCurrent->positional[index].name, pPositional[i], pCurrent->positional[index].type);
		if (!err.empty())
			return err;
	}

	/* check if the minimum required number of parameters has not been reached */
	if (pPositional.size() >= pCurrent->required)
		return L"";
	else if (pCurrent->name.empty())
		return L"Arguments missing.";
	return fmt::Str(L"Arguments missing for ", pGroupLower, " [", pCurrent->name, "].").wide();
}
std::wstring tools::Arguments::fVerifyOptional() {
	/* iterate over the optional arguments and verify them */
	for (auto& opt : pOptional) {
		/* check if the current group is not a user of the optional argument (current cannot be null) */
		if (!pNullGroup && opt.second.explicitUsage && opt.second.users.count(pCurrent->name) == 0) {
			if (!opt.second.parsed.empty())
				return fmt::Str(L"Argument [", opt.second.name, "] not meant for ", pGroupLower, " [", pCurrent->name, "].").wide();
			continue;
		}

		/* check if the optional-argument has been found */
		if (opt.second.parsed.empty()) {
			if (!opt.second.required)
				continue;
			return fmt::Str(L"Required argument [", opt.second.name, "] missing.").wide();
		}

		/* check if too many instances were found */
		if (opt.second.parsed.size() > 1 && !opt.second.multiple)
			return fmt::Str(L"Argument [", opt.second.name, "] can only be specified once.").wide();

		/* verify the values themselves */
		for (size_t i = 0; i < opt.second.parsed.size(); ++i) {
			std::wstring err = fParseValue(opt.second.name, opt.second.parsed[i], opt.second.type);
			if (!err.empty())
				return err;
		}
	}
	return L"";
}

void tools::Arguments::configure(const std::wstring& version, const std::wstring& desc, const wchar_t* groupNameLower, const wchar_t* groupNameUpper) {
	pVersion = version;
	pDescription = desc;
	pGroupLower = groupNameLower;
	pGroupUpper = groupNameUpper;
}
void tools::Arguments::addGlobalHelp(const std::wstring& name, const std::wstring& desc) {
	if (!name.empty())
		pHelpContent.push_back({ name, desc });
}

void tools::Arguments::addFlag(const std::wstring& name, wchar_t abbr, const std::wstring& desc, bool explicitUsage) {
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
	entry.explicitUsage = explicitUsage;
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = false;
	entry.versionFlag = false;
}
void tools::Arguments::addOption(const std::wstring& name, wchar_t abbr, const std::wstring& payload, bool multiple, bool required, const std::variant<tools::ArgType, tools::ArgEnum>& type, const std::wstring& desc, bool explicitUsage) {
	if (pOptional.count(name) > 0 || name.empty() || payload.empty())
		return;
	if (std::holds_alternative<tools::ArgEnum>(type) && std::get<tools::ArgEnum>(type).empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = payload;
	entry.description = desc;
	entry.explicitUsage = explicitUsage;
	entry.abbreviation = abbr;
	entry.type = type;
	entry.multiple = multiple;
	entry.required = required;
	entry.helpFlag = false;
	entry.versionFlag = false;
}
void tools::Arguments::addHelpFlag(const std::wstring& name, wchar_t abbr) {
	if (pOptional.count(name) > 0 || name.empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = L"";
	entry.explicitUsage = false;
	entry.description = L"Print this help menu.";
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = true;
}
void tools::Arguments::addVersionFlag(const std::wstring& name, wchar_t abbr) {
	if (pOptional.count(name) > 0 || name.empty())
		return;

	/* insert the new entry and insert the reference to the abbreviations */
	OptArg& entry = pOptional.insert({ name, OptArg() }).first->second;
	if (abbr != L'\0' && pAbbreviations.count(abbr) == 0)
		pAbbreviations.insert({ abbr, &entry });

	/* populate the new argument-entry */
	entry.name = name;
	entry.payload = L"";
	entry.explicitUsage = false;
	entry.description = L"Print the current program version.";
	entry.abbreviation = abbr;
	entry.multiple = true;
	entry.required = false;
	entry.helpFlag = false;
	entry.versionFlag = true;
}

void tools::Arguments::addGroup(const std::wstring& name, size_t required, bool lastCatchAll, const std::wstring& desc) {
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
void tools::Arguments::addPositional(const std::wstring& name, const std::variant<tools::ArgType, tools::ArgEnum>& type, const std::wstring& desc) {
	if (name.empty() || pGroupInsert == 0)
		return;
	if (std::holds_alternative<tools::ArgEnum>(type) && std::get<tools::ArgEnum>(type).empty())
		return;
	PosArg& entry = pGroupInsert->positional.emplace_back();

	/* populate the new positional-entry */
	entry.name = name;
	entry.description = desc;
	entry.type = type;
}
void tools::Arguments::addGroupHelp(const std::wstring& name, const std::wstring& desc) {
	if (!name.empty() && pGroupInsert != 0)
		pGroupInsert->helpContent.push_back({ name, desc });
}
void tools::Arguments::groupBindArgsOrOptions(const std::set<std::wstring>& names) {
	/* for null-groups, the concept of users does not exist, as the flags/optionals are all used by the single group */
	if (names.empty() || pGroupInsert == 0 || pNullGroup)
		return;

	/* iterate over the names and add the current group as users to them */
	for (const auto& name : names) {
		auto it = pOptional.find(name);
		if (it == pOptional.end() || !it->second.explicitUsage)
			continue;
		it->second.users.insert(pGroupInsert->name);
	}
}

std::pair<std::wstring, bool> tools::Arguments::parse(int argc, const char* const* argv) {
	if (argc == 0)
		return { L"Malformed arguments with no program-name.", false };
	if (pGroups.empty() || pVersion.empty())
		return { L"Misconfigured arguments-parser.", false };

	/* reset the state of the last parsing */
	pCurrent = 0;
	pPositional.clear();
	for (auto& opt : pOptional)
		opt.second.parsed.clear();

	/* fetch the current program name */
	size_t begin = 0;
	for (size_t i = 0; argv[0][i] != '\0'; ++i)
		begin = ((argv[0][i] == '/' || argv[0][i] == L'\\') ? i + 1 : begin);
	if (argv[0][begin] == '\0')
		return { fmt::Str("Malformed program path argument [", argv[0], "] encountered.").wide(), false };
	pProgram = fmt::Str(std::string_view(argv[0] + begin)).wide();

	/* setup the arguments-state */
	ArgState state;
	for (size_t i = 1; i < argc; ++i)
		state.args.push_back(fmt::Str(argv[i]).wide());

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
					return { fmt::Str("Malformed payload assigned to optional argument [", next, "].").wide(), false };
				payload = next.substr(i + 1);
				end = i;
				break;
			}

			/* parse the single long or multiple short arguments */
			size_t hypenCount = (next.size() > 2 && next[1] == L'-' ? 2 : 1);
			std::wstring err = fParseOptional(next.substr(hypenCount, end - hypenCount), payload, (hypenCount == 2), state);
			if (!err.empty())
				return { err, false };
			continue;
		}

		/* check if this is the group-selector */
		if (pCurrent == 0) {
			/* find the group with the matching argument-name */
			auto it = pGroups.find(next);

			/* check if a group has been found */
			if (it == pGroups.end())
				return { fmt::Str("Unknown ", pGroupLower, " [", next, "] encountered.").wide(), false };
			pCurrent = &it->second;
			continue;
		}

		/* add the argument to the list of positional arguments (dont perform any validations or limit checks for now) */
		pPositional.emplace_back() = json::Str(next);
	}

	/* check if the version or help should be printed */
	if (state.printHelp)
		return { fBuildHelpString(), true };
	if (state.printVersion)
		return { fBuildVersionString(), true };

	/* verify the positional arguments */
	if (std::wstring err = fVerifyPositional(); !err.empty())
		return { err, false };

	/* verify the optional arguments */
	if (std::wstring err = fVerifyOptional(); !err.empty())
		return { err, false };
	return { L"", true };
}

bool tools::Arguments::flag(const std::wstring& name) const {
	return !pOptional.at(name).parsed.empty();
}
const std::wstring& tools::Arguments::group() const {
	return pCurrent->name;
}
std::wstring tools::Arguments::helpHint() const {
	return fmt::Str("Try '", pProgram, " --help' for more information.").wide();
}

size_t tools::Arguments::options(const std::wstring& name) const {
	return pOptional.at(name).parsed.size();
}
std::optional<json::Value> tools::Arguments::option(const std::wstring& name, size_t index) const {
	if (index >= pOptional.at(name).parsed.size())
		return {};
	return pOptional.at(name).parsed[index];
}
size_t tools::Arguments::positionals() const {
	return pPositional.size();
}
std::optional<json::Value> tools::Arguments::positional(size_t index) const {
	if (index >= pPositional.size())
		return {};
	return pPositional[index];
}
