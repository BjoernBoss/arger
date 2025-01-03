/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"
#include "arger-parsed.h"
#include "arger-parser.h"
#include "arger-verify.h"
#include "arger-help.h"

namespace arger {
	/* convenience function to prepare the arguments */
	inline std::vector<std::wstring> Prepare(int argc, const str::IsChar auto* const* argv) {
		std::vector<std::wstring> args;
		for (size_t i = 0; i < argc; ++i)
			args.push_back(str::wd::To(argv[i]));
		return args;
	}
	inline std::vector<std::wstring> Prepare(const str::IsStr auto& line) {
		using ChType = str::StringChar<decltype(line)>;
		std::vector<std::wstring> args;
		std::basic_string_view<ChType> view{ line };

		/* split the string */
		wchar_t inStr = 0;
		bool lastWhitespace = true;
		for (size_t i = 0; i < view.size(); ++i) {
			/* check if the character is whitespace and a new argument needs to be
			*	started or if it can just be written out, as its part of a string */
			if (std::iswspace(view[i])) {
				if (inStr != 0)
					args.back().push_back(view[i]);
				else
					lastWhitespace = true;
				continue;
			}
			if (lastWhitespace)
				args.emplace_back();
			lastWhitespace = false;

			/* check if the next character is escaped */
			if (view[i] == L'\\') {
				if (++i >= view.size())
					break;
				args.back().push_back(view[i]);
			}

			/* check if a string is being ended or continued */
			else if (inStr != L'\0') {
				if (view[i] == inStr)
					inStr = L'\0';
				else
					args.back().push_back(view[i]);
			}

			/* check if a string is being started */
			else if (view[i] == L'\'' || view[i] == L'\"')
				inStr = view[i];
			else
				args.back().push_back(view[i]);
		}
		return args;
	}

	/* convenience functions for help-hints with default argument pattern */
	inline constexpr std::wstring HelpHint(const str::IsStr auto& line, const arger::Config& config) {
		return arger::HelpHint(arger::Prepare(line), config);
	}
	inline constexpr std::wstring HelpHint(int argc, const str::IsChar auto* const* argv, const arger::Config& config) {
		return arger::HelpHint({ str::wd::To(argc == 0 ? "" : argv[0]) }, config);
	}

	/* convenience functions for standard program arguments parsing */
	inline arger::Parsed Parse(const str::IsStr auto& line, const arger::Config& config) {
		return arger::Parse(arger::Prepare(line), config);
	}
	inline arger::Parsed Parse(int argc, const str::IsChar auto* const* argv, const arger::Config& config) {
		return arger::Parse(arger::Prepare(argc, argv), config);
	}

	/* convenience functions for menu-input arguments parsing */
	inline arger::Parsed Menu(const str::IsStr auto& line, const arger::Config& config) {
		return arger::Menu(arger::Prepare(line), config);
	}
	inline arger::Parsed Menu(int argc, const str::IsChar auto* const* argv, const arger::Config& config) {
		return arger::Menu(arger::Prepare(argc, argv), config);
	}
}
