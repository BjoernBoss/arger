/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2026 Bjoern Boss Henrichsen */
#pragma once

#include "arger-common.h"
#include "arger-config.h"
#include "arger-parsed.h"
#include "arger-parser.h"
#include "arger-verify.h"
#include "arger-help.h"

namespace arger {
	/* convenience function to prepare the arguments */
	inline std::vector<std::string> Prepare(size_t argc, const str::IsChar auto* const* argv) {
		std::vector<std::string> args;
		for (size_t i = 0; i < argc; ++i)
			args.push_back(str::ch::Safe(argv[i]));
		return args;
	}

	/* split the argument line into the list of separate arguments */
	inline std::vector<std::string> Prepare(const str::IsStr auto& line) {
		std::vector<std::string> args;

		/* split the string */
		char32_t inStr = U'\0';
		bool lastWhitespace = true, lastEscape = false;
		for (char32_t cp : str::CPRange{ line }) {
			/* check if the last character was escaped, in which case the codepoint can just be added */
			if (lastEscape) {
				lastEscape = false;
				str::CodepointTo(args.back(), cp);
				continue;
			}

			/* check if the character is whitespace and a new argument needs to be
			*	started or if it can just be written out, as its part of a string */
			if (cp::prop::IsSpace(cp)) {
				if (inStr != 0)
					str::CodepointTo(args.back(), cp);
				else
					lastWhitespace = true;
				continue;
			}
			if (lastWhitespace)
				args.emplace_back();
			lastWhitespace = false;

			/* check if the next character is escaped */
			if (cp == U'\\')
				lastEscape = true;

			/* check if a string is being ended or continued */
			else if (inStr != U'\0') {
				if (cp == inStr)
					inStr = U'\0';
				else
					str::CodepointTo(args.back(), cp);
			}

			/* check if a string is being started */
			else if (cp == U'\'' || cp == U'\"')
				inStr = cp;
			else
				str::CodepointTo(args.back(), cp);
		}
		return args;
	}

	/* convenience functions for help-hints with default argument pattern from a single command-line */
	inline constexpr std::string HelpHint(const str::IsStr auto& line, const arger::Config& config) {
		return arger::HelpHint(arger::Prepare(line), config);
	}

	/* convenience functions for help-hints with default argument pattern from separated arguments */
	inline constexpr std::string HelpHint(size_t argc, const str::IsChar auto* const* argv, const arger::Config& config) {
		return arger::HelpHint({ argc == 0 ? "" : str::ch::Safe(argv[0]) }, config);
	}

	/* convenience functions for standard program or menu-input arguments parsing from a single command-line */
	inline arger::Parsed Parse(const str::IsStr auto& line, const arger::Config& config, size_t lineLength = arger::NumCharsHelp) {
		return arger::Parse(arger::Prepare(line), config, lineLength);
	}

	/* convenience functions for standard program or menu-input arguments parsing from separated arguments */
	inline arger::Parsed Parse(size_t argc, const str::IsChar auto* const* argv, const arger::Config& config, size_t lineLength = arger::NumCharsHelp) {
		return arger::Parse(arger::Prepare(argc, argv), config, lineLength);
	}
}
