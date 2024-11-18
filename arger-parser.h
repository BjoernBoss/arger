/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"
#include "arger-help.h"

namespace arger {
	namespace detail {
		class Parser {
		private:
			std::vector<std::wstring> pArgs;
			detail::ValidConfig pConfig;
			const detail::ValidGroup* pSelected = 0;
			arger::Parsed pParsed;
			size_t pIndex = 0;
			bool pPrintHelp = false;
			bool pPrintVersion = false;
			bool pPositionalLocked = false;

		public:
			Parser(std::vector<std::wstring> args) : pArgs{ args } {}

		private:
			constexpr void fParseValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const {
				/* check if an enum was expected */
				if (std::holds_alternative<arger::Enum>(type)) {
					const arger::Enum& expected = std::get<arger::Enum>(type);
					if (expected.count(value.str()) != 0)
						return;
					throw arger::ParsingException{ L"Invalid enum for argument [", name, L"] encountered." };
				}

				/* validate the expected type and found value */
				switch (std::get<arger::Primitive>(type)) {
				case arger::Primitive::inum: {
					auto [num, len, res] = str::ParseNum<int64_t>(value.str(), 10, str::PrefixMode::overwrite);
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid signed integer for argument [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::unum: {
					auto [num, len, res] = str::ParseNum<uint64_t>(value.str(), 10, str::PrefixMode::overwrite);
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid unsigned integer for argument [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::real: {
					auto [num, len, res] = str::ParseNum<double>(value.str(), 10, str::PrefixMode::overwrite);
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid real for argument [", name, L"] encountered." };
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
					throw arger::ParsingException{ L"Invalid boolean for argument [", name, L"] encountered." };
				}
				case arger::Primitive::any:
				default:
					break;
				}
			}
			void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName) {
				bool payloadUsed = false;

				/* iterate over the list of optional abbreviations/single full-name and process them */
				for (size_t i = 0; i < arg.size(); ++i) {
					detail::ValidOption* entry = 0;

					/* resolve the optional-argument entry, depending on it being a short abbreviation, or a full name */
					if (fullName) {
						auto it = pConfig.options.find(arg);
						if (it == pConfig.options.end())
							throw arger::ParsingException{ L"Unknown optional argument [", arg, L"] encountered." };
						entry = &it->second;
						i = arg.size();
					}
					else {
						auto it = pConfig.abbreviations.find(arg[i]);
						if (it == pConfig.abbreviations.end())
							throw arger::ParsingException{ L"Unknown optional argument-abbreviation [", arg[i], L"] encountered." };
						entry = it->second;
					}

					/* check if this is a flag and mark it as seen and check if its a special purpose argument */
					if (!entry->payload) {
						pParsed.pFlags.insert(entry->option->name);
						if (entry->option->specialPurpose.help)
							pPrintHelp = true;
						else if (entry->option->specialPurpose.version)
							pPrintVersion = true;
						continue;
					}

					/* check if the payload has already been consumed */
					if (payloadUsed || (payload.empty() && pIndex >= pArgs.size()))
						throw arger::ParsingException{ L"Value [", entry->payload, L"] missing for optional argument [", entry->option->name, L"]." };
					payloadUsed = true;

					/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
					auto it = pParsed.pOptions.find(entry->option->name);
					if (it == pParsed.pOptions.end())
						it = pParsed.pOptions.insert({ entry->option->name, {} }).first;
					it->second.emplace_back(arger::Value{ payload.empty() ? pArgs[pIndex++] : payload });
				}

				/* check if a payload was supplied but not consumed */
				if (!payload.empty() && !payloadUsed)
					throw arger::ParsingException{ L"Value [", payload, L"] not used by optional arguments." };
			}

		private:
			constexpr void fVerifyPositional() {
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);

				/* check if the topmost object is still incomplete */
				if (topMost->incomplete)
					throw arger::ParsingException{ str::View{ topMost->groupName }.title(), L" missing." };

				/* validate the requirements for the positional arguments and parse their values */
				for (size_t i = 0; i < pParsed.pPositional.size(); ++i) {
					/* check if the argument is out of range */
					if (topMost->args->positionals.empty() || (topMost->maximum > 0 && i >= topMost->maximum)) {
						if (pSelected == 0)
							throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered." };
						throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered for ", topMost->super->groupName, L" [", pSelected->group->name, L"]." };
					}

					/* parse the argument */
					size_t index = std::min<size_t>(i, topMost->args->positionals.size() - 1);
					fParseValue(topMost->args->positionals[index].name, pParsed.pPositional[i], topMost->args->positionals[index].type);
				}

				/* check if the minimum required number of parameters has not been reached
				*	(maximum not necessary to be checked, as it will be checked implicitly by the verification-loop) */
				if (pParsed.pPositional.size() < topMost->minimum) {
					size_t index = std::min<size_t>(topMost->args->positionals.size() - 1, pParsed.pPositional.size());
					if (pSelected == 0)
						throw arger::ParsingException{ L"Argument [", topMost->args->positionals[index].name, L"] missing." };
					throw arger::ParsingException{ L"Argument [", topMost->args->positionals[index].name, L"] missing for ", topMost->super->groupName, L" [", pSelected->group->name, L"]." };
				}
			}
			void fVerifyOptional() {
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);

				/* iterate over the optional arguments and verify them */
				for (auto& [name, option] : pConfig.options) {
					/* check if the current group is not a user of the optional argument (restricted can
					*	only be true if groups exist and the null-group is contained at all times) */
					if (option.restricted && !option.users.contains(pSelected)) {
						if (option.payload ? pParsed.pFlags.contains(name) : pParsed.pOptions.contains(name))
							throw arger::ParsingException{ L"Argument [", name, L"] not meant for ", topMost->super->groupName, L" [", pSelected->group->name, L"]." };
						continue;
					}

					/* check if this is a flag, in which case nothing more needs to be checked */
					if (!option.payload)
						continue;

					/* lookup the option */
					auto it = pParsed.pOptions.find(name);
					size_t count = (it == pParsed.pOptions.end() ? 0 : it->second.size());

					/* check if the optional-argument has been found */
					if (option.minimum > count)
						throw arger::ParsingException{ L"Argument [", name, L"] missing." };

					/* check if too many instances were found */
					if (option.maximum > 0 && count > option.maximum)
						throw arger::ParsingException{ L"Argument [", name, L"] can at most be specified ", option.maximum, " times." };

					/* verify the values themselves */
					for (size_t i = 0; i < count; ++i)
						fParseValue(name, it->second[i], option.option->payload.type);
				}
			}
			void fRecCheckConstraints(const detail::ValidArguments* args) {
				if (args == 0)
					return;
				fRecCheckConstraints(args->super);

				for (const auto& fn : args->args->constraints) {
					std::wstring err = fn(pParsed);
					if (!err.empty())
						throw arger::ParsingException{ err };
				}
			}

		public:
			arger::Parsed parse(const arger::Config& config) {
				const detail::ValidArguments* topMost = static_cast<const detail::ValidArguments*>(&pConfig);

				/* validate and pre-process the configuration */
				detail::ValidateConfig(config, pConfig);

				/* parse the program name */
				detail::BaseBuilder base{ pArgs.empty() ? L"" : pArgs[pIndex++], config };

				/* iterate over the arguments and parse them based on the definitions */
				size_t dirtyGroup = pArgs.size();
				while (pIndex < pArgs.size()) {
					const std::wstring& next = pArgs[pIndex++];

					/* check if its an optional argument or positional-lock */
					if (!pPositionalLocked && !next.empty() && next[0] == L'-') {
						if (next == L"--") {
							pPositionalLocked = true;
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
								throw arger::ParsingException{ L"Malformed payload assigned to optional argument [", next, L"]." };
							payload = next.substr(i + 1);
							end = i;
							break;
						}

						/* parse the single long or multiple short arguments */
						size_t hypenCount = (next.size() > 2 && next[1] == L'-' ? 2 : 1);
						fParseOptional(next.substr(hypenCount, end - hypenCount), payload, (hypenCount == 2));
						continue;
					}

					/* check if this is a group-selector */
					if (topMost->incomplete && dirtyGroup == pArgs.size()) {
						/* find the group with the matching argument-name */
						auto it = topMost->sub.find(next);

						/* check if a group has been found */
						if (it != topMost->sub.end()) {
							topMost = (pSelected = &it->second);
							continue;
						}
						dirtyGroup = pIndex - 1;
					}

					/* add the argument to the list of positional arguments (dont perform any validations or
					*	limit checks for now, but defer it until the help/version string have been potentially
					*	printed, in order for help to be printed without the arguments being valid) */
					pParsed.pPositional.emplace_back(arger::Value{ next });
				}

				/* check if the version or help should be printed */
				if (pPrintHelp)
					throw arger::PrintMessage{ detail::HelpBuilder{ base, pConfig, pSelected }.buildHelpString() };
				if (pPrintVersion)
					throw arger::PrintMessage{ base.buildVersionString() };

				/* verify the group selection */
				if (dirtyGroup < pArgs.size())
					throw arger::ParsingException{ L"Unknown ", topMost->groupName, L" [", pArgs[dirtyGroup], L"] encountered." };

				/* verify the positional arguments */
				fVerifyPositional();

				/* verify the optional arguments */
				fVerifyOptional();

				/* setup the selected group */
				pParsed.pGroupId = (pSelected == 0 ? L"" : pSelected->id);

				/* validate all root/selection constraints */
				fRecCheckConstraints(topMost);

				/* validate all optional constraints */
				for (const auto& [name, option] : pConfig.options) {
					if (!pParsed.pOptions.contains(name))
						continue;
					for (const auto& fn : option.option->constraints) {
						std::wstring err = fn(pParsed);
						if (!err.empty())
							throw arger::ParsingException{ err };
					}
				}

				/* return the parsed structure */
				arger::Parsed out;
				std::swap(out, pParsed);
				return out;
			}
		};
	}

	inline arger::Parsed Parse(int argc, const char* const* argv, const arger::Config& config) {
		/* convert the arguments */
		std::vector<std::wstring> args;
		for (size_t i = 0; i < argc; ++i)
			args.push_back(str::wd::To(argv[i]));

		/* parse the actual arguments based on the configuration */
		return detail::Parser{ args }.parse(config);
	}
	inline arger::Parsed Parse(int argc, const wchar_t* const* argv, const arger::Config& config) {
		/* convert the arguments */
		std::vector<std::wstring> args;
		for (size_t i = 0; i < argc; ++i)
			args.emplace_back(argv[i]);

		/* parse the actual arguments based on the configuration */
		return detail::Parser{ args }.parse(config);
	}
}
