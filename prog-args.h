#pragma once

#include <ustring/ustring.h>

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <cwctype>
#include <variant>
#include <optional>
#include <set>
#include <stdexcept>

namespace args {
	enum class Primitive : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	using Enum = std::map<std::wstring, std::wstring>;
	using Type = std::variant<args::Primitive, args::Enum>;

	/* exception thrown when accessing an args::Value as a certain type, which it is not */
	class ArgsTypeException : public std::runtime_error {
	public:
		ArgsTypeException(const std::string& s) : runtime_error(s) {}
	};

	/* representation of a single argument value (performs primitive type-conversions when accessing values) */
	struct Value : private std::variant<uint64_t, int64_t, double, bool, std::wstring> {
	private:
		using Parent = std::variant<uint64_t, int64_t, double, bool, std::wstring>;

	public:
		constexpr Value(args::Value&&) = default;
		constexpr Value(const args::Value&) = default;
		constexpr args::Value& operator=(args::Value&&) = default;
		constexpr args::Value& operator=(const args::Value&) = default;

	public:
		constexpr Value(uint64_t v) : Parent{ v } {}
		constexpr Value(int64_t v) : Parent{ v } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(double v) : Parent{ v } {}
		constexpr Value(bool v) : Parent{ v } {}
		constexpr Value(std::wstring&& v) : Parent{ std::move(v) } {}
		constexpr Value(const std::wstring& v) : Parent{ v } {}

	public:
		constexpr bool isUNum() const {
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isINum() const {
			if (std::holds_alternative<int64_t>(*this))
				return true;
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isReal() const {
			if (std::holds_alternative<double>(*this))
				return true;
			if (std::holds_alternative<int64_t>(*this))
				return true;
			return std::holds_alternative<uint64_t>(*this);
		}
		constexpr bool isBool() const {
			return std::holds_alternative<bool>(*this);
		}
		constexpr bool isStr() const {
			return std::holds_alternative<std::wstring>(*this);
		}

	public:
		constexpr uint64_t unum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return std::get<uint64_t>(*this);
			throw args::ArgsTypeException("args::Value is not an unsigned-number");
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			throw args::ArgsTypeException("args::Value is not a signed-number");
		}
		constexpr double real() const {
			if (std::holds_alternative<double>(*this))
				return std::get<double>(*this);
			if (std::holds_alternative<uint64_t>(*this))
				return double(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return double(std::get<int64_t>(*this));
			throw args::ArgsTypeException("args::Value is not a real");
		}
		constexpr bool boolean() const {
			if (std::holds_alternative<bool>(*this))
				return std::get<bool>(*this);
			throw args::ArgsTypeException("args::Value is not a boolean");
		}
		constexpr const std::wstring& str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return std::get<std::wstring>(*this);
			throw args::ArgsTypeException("args::Value is not a string");
		}
	};

	class ArgParser {
	private:
		static constexpr size_t NumCharsHelpLeft = 32;
		static constexpr size_t NumCharsHelp = 100;

	private:
		struct OptArg {
			std::wstring name;
			std::wstring payload;
			std::wstring description;
			std::vector<args::Value> parsed;
			std::set<std::wstring> users;
			args::Type type;
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
			args::Type type;
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
		std::vector<args::Value> pPositional;
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
		void fBuildHelpAddEnumDescription(const args::Type& type, HelpState& s) const;
		const wchar_t* fBuildHelpArgValueString(const args::Type& type) const;
		std::wstring fBuildHelpString() const;
		std::wstring fBuildVersionString() const;

	private:
		std::wstring fParseValue(const std::wstring& name, args::Value& value, const args::Type& type) const;
		std::wstring fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s);
		std::wstring fVerifyPositional();
		std::wstring fVerifyOptional();

	public:
		void configure(const std::wstring& version, const std::wstring& desc, const wchar_t* groupNameLower = L"mode", const wchar_t* groupNameUpper = L"Mode");
		void addGlobalHelp(const std::wstring& name, const std::wstring& desc);

	public:
		void addFlag(const std::wstring& name, wchar_t abbr, const std::wstring& desc, bool explicitUsage);
		void addOption(const std::wstring& name, wchar_t abbr, const std::wstring& payload, bool multiple, bool required, const args::Type& type, const std::wstring& desc, bool explicitUsage);
		void addHelpFlag(const std::wstring& name, wchar_t abbr);
		void addVersionFlag(const std::wstring& name, wchar_t abbr);

	public:
		void addGroup(const std::wstring& name, size_t required, bool lastCatchAll, const std::wstring& desc);
		void addPositional(const std::wstring& name, const args::Type& type, const std::wstring& desc);
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
		std::optional<args::Value> option(const std::wstring& name, size_t index = 0) const;
		size_t positionals() const;
		std::optional<args::Value> positional(size_t index) const;
	};
}
