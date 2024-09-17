/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
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
	class Parsed;
	class Arguments;

	enum class Primitive : uint8_t {
		any,
		inum,
		unum,
		real,
		boolean
	};
	using Enum = std::map<std::wstring, std::wstring>;
	using Type = std::variant<arger::Primitive, arger::Enum>;

	using Constraint = std::function<std::wstring(const arger::Parsed&)>;

	/* exception thrown when accessing an arger::Value as a certain type, which it is not */
	struct TypeException : public str::BuildException {
		template <class... Args>
		constexpr TypeException(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when malformed or invalid arguments are encountered */
	struct ParsingException : public str::BuildException {
		template <class... Args>
		constexpr ParsingException(const Args&... args) : str::BuildException{ args... } {}
	};

	/* exception thrown when only a message should be printed but no */
	struct PrintMessage : public str::BuildException {
		template <class... Args>
		constexpr PrintMessage(const Args&... args) : str::BuildException{ args... } {}
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
			throw arger::TypeException(L"arger::Value is not an unsigned-number");
		}
		constexpr int64_t inum() const {
			if (std::holds_alternative<uint64_t>(*this))
				return int64_t(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return std::get<int64_t>(*this);
			throw arger::TypeException(L"arger::Value is not a signed-number");
		}
		constexpr double real() const {
			if (std::holds_alternative<double>(*this))
				return std::get<double>(*this);
			if (std::holds_alternative<uint64_t>(*this))
				return double(std::get<uint64_t>(*this));
			if (std::holds_alternative<int64_t>(*this))
				return double(std::get<int64_t>(*this));
			throw arger::TypeException(L"arger::Value is not a real");
		}
		constexpr bool boolean() const {
			if (std::holds_alternative<bool>(*this))
				return std::get<bool>(*this);
			throw arger::TypeException(L"arger::Value is not a boolean");
		}
		constexpr const std::wstring& str() const {
			if (std::holds_alternative<std::wstring>(*this))
				return std::get<std::wstring>(*this);
			throw arger::TypeException(L"arger::Value is not a string");
		}
	};

	/* represents the parsed results of the arguments */
	class Parsed {
		friend class arger::Arguments;
	private:
		std::set<std::wstring> pFlags;
		std::map<std::wstring, std::vector<arger::Value>> pOptions;
		std::vector<arger::Value> pPositional;
		std::wstring pGroup;

	public:
		bool flag(const std::wstring& name) const;
		const std::wstring& group() const;

	public:
		size_t options(const std::wstring& name) const;
		std::optional<arger::Value> option(const std::wstring& name, size_t index = 0) const;
		size_t positionals() const;
		std::optional<arger::Value> positional(size_t index) const;
	};

	namespace detail {
		struct Positional {
			std::wstring name;
			arger::Type type;
			std::wstring description;
		};
		struct Help {
			std::wstring name;
			std::wstring description;
		};
	}

	struct Configuration {
	public:
		std::set<std::wstring> users;
		std::vector<arger::Constraint> constraints;
		std::vector<detail::Positional> positional;
		std::vector<detail::Help> help;
		std::wstring description;
		std::wstring payload;
		arger::Type type;
		size_t minimum = 0;
		size_t maximum = 0;
		wchar_t abbreviation = 0;
		bool helpFlag = false;
		bool versionFlag = false;
		bool lastCatchAll = false;

	public:
		template <class RType>
		constexpr arger::Configuration& operator|(const RType& r) {
			r.apply(*this);
			return *this;
		}
	};

	namespace detail {
		template <class Type>
		struct ConfigBase {
			constexpr operator arger::Configuration() const {
				arger::Configuration config;
				static_cast<const Type&>(*this).apply(config);
				return config;
			}
			template <class RType>
			constexpr arger::Configuration operator|(const RType& r) const {
				arger::Configuration config;
				static_cast<const Type&>(*this).apply(config);
				r.apply(config);
				return config;
			}
		};
	}

	namespace conf {
		/* [options/groups] add description for the group/option */
		struct Description : public detail::ConfigBase<conf::Description> {
			std::wstring desc;
			constexpr Description(std::wstring desc) : desc{ desc } {}
			constexpr void apply(arger::Configuration& config) const {
				config.description = desc;
			}
		};

		/* [options/groups] configure the required counters (minium greater than 0 implies required; maximum of null implies no maximum) */
		struct Require : public detail::ConfigBase<conf::Require> {
			size_t minimum = 0;
			size_t maximum = 0;
			constexpr Require(size_t min = 1, size_t max = 0) : minimum{ min }, maximum{ max } {}
			constexpr void apply(arger::Configuration& config) const {
				config.minimum = minimum;
				config.maximum = maximum;
			}
		};

		/* [options] add a shortcut single character to reference this option */
		struct Abbreviation : public detail::ConfigBase<conf::Abbreviation> {
			wchar_t chr;
			constexpr Abbreviation(wchar_t c) : chr{ c } {}
			constexpr void apply(arger::Configuration& config) const {
				config.abbreviation = chr;
			}
		};

		/* [options] add a payload requirement to the option */
		struct Payload : public detail::ConfigBase<conf::Payload> {
			std::wstring name;
			arger::Type type;
			constexpr Payload(std::wstring name, arger::Type type) : name{ name }, type{ type } {}
			constexpr void apply(arger::Configuration& config) const {
				config.payload = name;
				config.type = type;
			}
		};

		/* [options] limit the option to a subset of defined groups */
		struct Limit : public detail::ConfigBase<conf::Limit> {
			std::set<std::wstring> users;
			Limit(std::set<std::wstring> names) : users{ names } {}
			void apply(arger::Configuration& config) const {
				config.users.insert(users.begin(), users.end());
			}
		};

		/* [global/options/groups] define callback-constraints to be executed (all globals or) if one or more of the groups/options have been passed in */
		struct Constraint : public detail::ConfigBase<conf::Constraint> {
			arger::Constraint constraint;
			Constraint(arger::Constraint con) : constraint{ con } {}
			constexpr void apply(arger::Configuration& config) const {
				config.constraints.push_back(constraint);
			}
		};

		/* [options] define this as the help-menu option-flag */
		struct HelpFlag : detail::ConfigBase<conf::HelpFlag> {
			constexpr HelpFlag() {}
			constexpr void apply(arger::Configuration& config) const {
				config.helpFlag = true;
			}
		};

		/* [options] define this as the version-menu option-flag */
		struct VersionFlag : detail::ConfigBase<conf::VersionFlag> {
			constexpr VersionFlag() {}
			constexpr void apply(arger::Configuration& config) const {
				config.versionFlag = true;
			}
		};

		/* [groups] define the last positional argument of the group to catch any remaining incoming arguments */
		struct LastCatchAll : detail::ConfigBase<conf::LastCatchAll> {
			constexpr LastCatchAll() {}
			constexpr void apply(arger::Configuration& config) const {
				config.lastCatchAll = true;
			}
		};

		/* [groups] add another positional argument */
		struct Positional : detail::ConfigBase<conf::Positional> {
			detail::Positional positional;
			constexpr Positional(std::wstring name, const arger::Type& type, std::wstring description = L"") : positional{ name, type, description } {}
			constexpr void apply(arger::Configuration& config) const {
				config.positional.push_back(positional);
			}
		};

		/* [global/groups] add another help-string to the given group or the global list */
		struct Help : detail::ConfigBase<conf::Help> {
			detail::Help help;
			constexpr Help(std::wstring name, std::wstring description) : help{ name, description } {}
			constexpr void apply(arger::Configuration& config) const {
				config.help.push_back(help);
			}
		};
	}

	class Arguments {
	private:
		static constexpr size_t NumCharsHelpLeft = 32;
		static constexpr size_t NumCharsHelp = 100;

	private:
		struct OptionalEntry {
			std::wstring name;
			std::wstring payload;
			std::wstring description;
			std::vector<arger::Constraint> constraints;
			std::set<std::wstring> users;
			size_t minimum = 0;
			size_t maximum = 0;
			arger::Type type;
			wchar_t abbreviation = 0;
			bool helpFlag = false;
			bool versionFlag = false;
		};
		struct GroupEntry {
			std::wstring name;
			std::wstring description;
			std::vector<detail::Help> helpContent;
			std::vector<detail::Positional> positional;
			std::vector<arger::Constraint> constraints;
			size_t minimum = 0;
			bool catchAll = false;
		};
		struct HelpState {
			std::wstring buffer;
			size_t pos = 0;
		};
		struct ArgState {
			std::vector<std::wstring> args;
			GroupEntry* current = 0;
			arger::Parsed parsed;
			std::wstring program;
			size_t index = 0;
			bool printHelp = false;
			bool printVersion = false;
			bool positionalLocked = false;
		};

	private:
		std::wstring pVersion;
		std::wstring pDescription;
		std::wstring pGroupName;
		std::vector<detail::Help> pHelpContent;
		std::vector<arger::Constraint> pConstraints;
		std::map<std::wstring, GroupEntry> pGroups;
		std::map<std::wstring, OptionalEntry> pOptional;
		std::map<wchar_t, OptionalEntry*> pAbbreviations;
		bool pNullGroup = false;

	public:
		Arguments(std::wstring version, std::wstring desc, std::wstring groupName = L"mode");

	private:
		void fAddHelpNewLine(bool emptyLine, HelpState& s) const;
		void fAddHelpToken(const std::wstring& add, HelpState& s) const;
		void fAddHelpSpacedToken(const std::wstring& add, HelpState& s) const;
		void fAddHelpString(const std::wstring& add, HelpState& s, size_t offset = 0, size_t indentAutoBreaks = 0) const;

	private:
		void fBuildHelpUsage(const GroupEntry* current, const std::wstring& program, HelpState& s) const;
		void fBuildHelpAddHelpContent(const std::vector<detail::Help>& content, HelpState& s) const;
		void fBuildHelpAddOptionals(bool required, const GroupEntry* current, HelpState& s) const;
		void fBuildHelpAddEnumDescription(const arger::Type& type, HelpState& s) const;
		const wchar_t* fBuildHelpArgValueString(const arger::Type& type) const;
		std::wstring fBuildHelpString(const GroupEntry* current, const std::wstring& program) const;
		std::wstring fBuildVersionString(const std::wstring& program) const;

	private:
		void fParseValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const;
		void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, ArgState& s);
		void fVerifyPositional(ArgState& s);
		void fVerifyOptional(ArgState& s);
		arger::Parsed fParseArgs(std::vector<std::wstring> args);

	public:
		void addGlobal(const arger::Configuration& config);
		void addOption(const std::wstring& name, const arger::Configuration& config);
		void addGroup(const std::wstring& name, const arger::Configuration& config);

	public:
		arger::Parsed parse(int argc, const char* const* argv);
		arger::Parsed parse(int argc, const wchar_t* const* argv);
	};

	std::wstring HelpHint(int argc, const char* const* argv);
	std::wstring HelpHint(int argc, const wchar_t* const* argv);
}
