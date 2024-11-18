/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"
#include "arger-help.h"

namespace arger {
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
			size_t maximum = 0;
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
		Arguments(std::wstring version, std::wstring groupName = L"mode");

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
		std::wstring fBuildHelpLimitString(size_t minimum, size_t maximum) const;
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

	inline arger::Parsed Parse(int argc, const char* const* argv, const arger::_Config& config) {
		std::vector<std::wstring> args;

		/* validate the configuration */
		detail::ValidConfig validated;
		detail::ValidateConfig(config, validated);

		/* convert the arguments and parse them */
		for (size_t i = 0; i < argc; ++i)
			args.push_back(str::wd::To(argv[i]));
		return {};
	}
	inline arger::Parsed Parse(int argc, const wchar_t* const* argv, const arger::_Config& config) {
		std::vector<std::wstring> args;

		/* validate the configuration */
		detail::ValidConfig validated;
		detail::ValidateConfig(config, validated);

		/* convert the arguments and parse them */
		for (size_t i = 0; i < argc; ++i)
			args.emplace_back(argv[i]);
		return {};
	}

	std::wstring HelpHint(int argc, const char* const* argv);
	std::wstring HelpHint(int argc, const wchar_t* const* argv);
}
