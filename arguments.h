#pragma once

#include <jsonify/jsonify.h>
#include "str-fmt.h"

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <cwctype>
#include <variant>
#include <optional>
#include <set>

namespace tools {
	struct Failed {
	public:
		bool showHelp = false;

	public:
		Failed() = delete;
		Failed(bool help) : showHelp(help) {}
	};
	enum class ArgType : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	using ArgEnum = std::map<std::wstring, std::wstring>;

	class Arguments {
	private:
		static constexpr size_t NumCharsHelpLeft = 32;
		static constexpr size_t NumCharsHelp = 100;

	private:
		using ArgValue = std::variant<tools::ArgType, tools::ArgEnum>;
		struct OptArg {
			std::wstring name;
			std::wstring payload;
			std::wstring description;
			std::vector<json::Value> parsed;
			std::set<std::wstring> users;
			ArgValue type;
			wchar_t abbreviation = 0;
			bool explicitUsage = false;
			bool multiple = false;
			bool required = false;
			bool helpFlag = false;
			bool versionFlag = false;
		};
		struct PosArg {
			std::wstring name;
			std::wstring description;
			ArgValue type;
		};
		struct HelpEntry {
			std::wstring name;
			std::wstring description;
		};
		struct GroupEntry {
			std::wstring name;
			std::wstring description;
			std::vector<HelpEntry> helpContent;
			std::vector<PosArg> positional;
			size_t required = 0;
			bool catchAll = false;
		};
		struct HelpState {
			std::wstring buffer;
			size_t pos = 0;
		};
		struct ArgState {
			std::vector<std::wstring> args;
			size_t index = 0;
			bool printHelp = false;
			bool printVersion = false;
			bool positionalLocked = false;
		};

	private:
		std::wstring pVersion;
		std::wstring pDescription;
		std::wstring pProgram;
		std::vector<json::Value> pPositional;
		std::vector<HelpEntry> pHelpContent;
		std::map<std::wstring, GroupEntry> pGroups;
		std::map<std::wstring, OptArg> pOptional;
		std::map<wchar_t, OptArg*> pAbbreviations;
		const wchar_t* pGroupLower = L"mode";
		const wchar_t* pGroupUpper = L"Mode";
		GroupEntry* pGroupInsert = 0;
		GroupEntry* pCurrent = 0;
		bool pNullGroup = false;

	private:
		void fAddHelpNewLine(bool emptyLine, HelpState& s) const;
		void fAddHelpToken(const std::wstring& add, HelpState& s) const;
		void fAddHelpSpacedToken(const std::wstring& add, HelpState& s) const;
		void fAddHelpString(const std::wstring& add, HelpState& s, size_t offset = 0, size_t indentAutoBreaks = 0) const;

	private:
		void fBuildHelpUsage(HelpState& s) const;
		void fBuildHelpAddHelpContent(const std::vector<HelpEntry>& content, HelpState& s) const;
		void fBuildHelpAddOptionals(bool required, HelpState& s) const;
		void fBuildHelpAddEnumDescription(const ArgValue& value, HelpState& s) const;
		const wchar_t* fBuildHelpArgValueString(const ArgValue& type) const;
		std::wstring fBuildHelpString() const;
		std::wstring fBuildVersionString() const;

	private:
		std::wstring fParseValue(const std::wstring& name, json::Value& value, const ArgValue& type) const;
		std::wstring fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s);
		std::wstring fVerifyPositional();
		std::wstring fVerifyOptional();

	public:
		void configure(const std::wstring& version, const std::wstring& desc, const wchar_t* groupNameLower = L"mode", const wchar_t* groupNameUpper = L"Mode");
		void addGlobalHelp(const std::wstring& name, const std::wstring& desc);

	public:
		void addFlag(const std::wstring& name, wchar_t abbr, const std::wstring& desc, bool explicitUsage);
		void addOption(const std::wstring& name, wchar_t abbr, const std::wstring& payload, bool multiple, bool required, const std::variant<tools::ArgType, tools::ArgEnum>& type, const std::wstring& desc, bool explicitUsage);
		void addHelpFlag(const std::wstring& name, wchar_t abbr);
		void addVersionFlag(const std::wstring& name, wchar_t abbr);

	public:
		void addGroup(const std::wstring& name, size_t required, bool lastCatchAll, const std::wstring& desc);
		void addPositional(const std::wstring& name, const std::variant<tools::ArgType, tools::ArgEnum>& type, const std::wstring& desc);
		void addGroupHelp(const std::wstring& name, const std::wstring& desc);
		void groupBindArgsOrOptions(const std::set<std::wstring>& names);

	public:
		std::pair<std::wstring, bool> parse(int argc, const char* const* argv);

	public:
		/* must all first be called after calling Arguments::parse(...) */
		bool flag(const std::wstring& name) const;
		const std::wstring& group() const;
		std::wstring helpHint() const;

	public:
		size_t options(const std::wstring& name) const;
		std::optional<json::Value> option(const std::wstring& name, size_t index = 0) const;
		size_t positionals() const;
		std::optional<json::Value> positional(size_t index) const;
	};
}
