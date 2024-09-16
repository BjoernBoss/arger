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

namespace arger {
	class Parser;

	enum class Primitive : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	using Enum = std::map<std::wstring, std::wstring>;
	using Type = std::variant<arger::Primitive, arger::Enum>;

	using Constraint = std::function<std::wstring(const arger::Parser&)>;

	class ExceptionBase {
	private:
		std::wstring pMessage;

	public:
		constexpr ExceptionBase(const std::wstring& msg) : pMessage{ msg } {}
		constexpr const std::wstring& what() const {
			return pMessage;
		}
	};

	/* exception thrown when accessing an arger::Value as a certain type, which it is not */
	class ArgsTypeException : public arger::ExceptionBase {
	public:
		ArgsTypeException(const std::wstring& s) : arger::ExceptionBase{ s } {}
	};

	/* exception thrown when malformed or invalid arguments are encountered */
	class ArgsParsingException : public arger::ExceptionBase {
	public:
		ArgsParsingException(const std::wstring& s) : arger::ExceptionBase{ s } {}
	};

	/* exception thrown when only a message should be printed but no */
	class ArgPrintMessage : public arger::ExceptionBase {
	public:
		ArgPrintMessage(const std::wstring& s) : arger::ExceptionBase{ s } {}
	};

	/* representation of a single argument value (performs primitive type-conversions when accessing values) */
	struct Value : private std::variant<uint64_t, int64_t, double, bool, std::wstring> {
	private:
		using Parent = std::variant<uint64_t, int64_t, double, bool, std::wstring>;

	public:
		constexpr Value(arger::Value&&) = default;
		constexpr Value(const arger::Value&) = default;
		constexpr arger::Value& operator=(arger::Value&&) = default;
		constexpr arger::Value& operator=(const arger::Value&) = default;

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
		/* convenience */
		constexpr Value(int v) : Parent{ int64_t(v) } {
			if (v >= 0)
				static_cast<Parent&>(*this) = uint64_t(v);
		}
		constexpr Value(const wchar_t* s) : Parent{ std::wstring(s) } {}

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
			throw arger::ArgsTypeException(L"arger::Value is not an unsigned-number");
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			throw arger::ArgsTypeException(L"arger::Value is not a signed-number");
		}
		constexpr double real() const {
			if (std::holds_alternative<double>(*this))
				return std::get<double>(*this);
			if (std::holds_alternative<uint64_t>(*this))
				return double(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return double(std::get<int64_t>(*this));
			throw arger::ArgsTypeException(L"arger::Value is not a real");
		}
		constexpr bool boolean() const {
			if (std::holds_alternative<bool>(*this))
				return std::get<bool>(*this);
			throw arger::ArgsTypeException(L"arger::Value is not a boolean");
		}
		constexpr const std::wstring& str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return std::get<std::wstring>(*this);
			throw arger::ArgsTypeException(L"arger::Value is not a string");
		}
	};

	class Parser {
	private:
		static constexpr size_t NumCharsHelpLeft = 32;
		static constexpr size_t NumCharsHelp = 100;

	private:
		enum class BindType :uint8_t {
			none,
			argument,
			group,
			positional
		};
		struct OptArg {
			std::wstring name;
			std::wstring payload;
			std::wstring description;
			std::vector<arger::Value> parsed;
			std::set<std::wstring> users;
			arger::Type type;
			wchar_t abbreviation = 0;
			bool generalPurpose = false;
			bool multiple = false;
			bool required = false;
			bool helpFlag = false;
			bool versionFlag = false;
		};
		struct PosArg {
			std::wstring name;
			std::wstring description;
			arger::Type type;
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
		struct ConstraintEntry {
			arger::Constraint fn;
			std::wstring binding;
			size_t index = 0;
			BindType type = BindType::none;
		};

	private:
		std::wstring pVersion;
		std::wstring pDescription;
		std::wstring pProgram;
		std::wstring pGroupLower = L"mode";
		std::wstring pGroupUpper = L"Mode";
		std::vector<arger::Value> pPositional;
		std::vector<HelpEntry> pHelpContent;
		std::vector<ConstraintEntry> pConstraints;
		std::map<std::wstring, GroupEntry> pGroups;
		std::map<std::wstring, OptArg> pOptional;
		std::map<wchar_t, OptArg*> pAbbreviations;
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
		void fBuildHelpAddEnumDescription(const arger::Type& type, HelpState& s) const;
		const wchar_t* fBuildHelpArgValueString(const arger::Type& type) const;
		std::wstring fBuildHelpString() const;
		std::wstring fBuildVersionString() const;

	private:
		template <class... Args>
		arger::ArgsParsingException fException(Args&&... args) const {
			return arger::ArgsParsingException(str::Build<std::wstring>(std::forward<Args>(args)...));
		}
		void fParseValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const;
		void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s);
		void fVerifyPositional();
		void fVerifyOptional();
		void fParseName(const std::wstring& arg);
		void fParseArgs(std::vector<std::wstring> args);

	public:
		void configure(std::wstring version, std::wstring desc, std::wstring groupNameLower = L"mode", std::wstring groupNameUpper = L"Mode");
		void addGlobalHelp(std::wstring name, std::wstring desc);

	public:
		void addFlag(const std::wstring& name, wchar_t abbr, std::wstring desc, bool generalPurpose);
		void addOption(const std::wstring& name, wchar_t abbr, const std::wstring& payload, bool multiple, bool required, const arger::Type& type, std::wstring desc, bool generalPurpose);
		void addHelpFlag(const std::wstring& name, wchar_t abbr);
		void addVersionFlag(const std::wstring& name, wchar_t abbr);

	public:
		void addGroup(const std::wstring& name, size_t required, bool lastCatchAll, std::wstring desc);
		void addPositional(const std::wstring& name, const arger::Type& type, std::wstring desc);
		void addGroupHelp(const std::wstring& name, std::wstring desc);
		void bindSpecialFlagsOrOptions(const std::set<std::wstring>& names);

	public:
		void addConstraint(arger::Constraint fn);
		void addFlagOrOptionConstraint(const std::wstring& name, arger::Constraint fn);
		void addGroupConstraint(const std::wstring& name, arger::Constraint fn);
		void addPositionalConstraint(const std::wstring& name, size_t index, arger::Constraint fn);

	public:
		void parse(int argc, const char* const* argv);
		void parse(int argc, const wchar_t* const* argv);

	public:
		/* must all first be called after calling Arguments::parse(...) */
		bool flag(const std::wstring& name) const;
		const std::wstring& group() const;
		std::wstring helpHint() const;

	public:
		size_t options(const std::wstring& name) const;
		std::optional<arger::Value> option(const std::wstring& name, size_t index = 0) const;
		size_t positionals() const;
		std::optional<arger::Value> positional(size_t index) const;
	};
}
