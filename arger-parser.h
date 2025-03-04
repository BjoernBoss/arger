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
			const std::vector<std::wstring>& pArgs;
			detail::ValidConfig pConfig;
			const detail::ValidGroup* pSelected = 0;
			arger::Parsed pParsed;
			std::wstring pDeferred;
			size_t pIndex = 0;
			bool pPrintHelp = false;
			bool pPrintVersion = false;
			bool pPositionalLocked = false;

		public:
			Parser(const std::vector<std::wstring>& args) : pArgs{ args } {}

		private:
			void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, bool hasPayload, bool menu) {
				bool payloadUsed = false;

				/* check if it could be the help menu */
				size_t i = 0;
				if (fullName && !menu) {
					if (pConfig.help != 0 && pConfig.help->name == arg) {
						pPrintHelp = true;
						i = arg.size();
					}
					else if (pConfig.version != 0 && pConfig.version->name == arg) {
						pPrintVersion = true;
						i = arg.size();
					}
				}

				/* iterate over the list of optional abbreviations/single full-name and process them */
				for (; i < arg.size(); ++i) {
					detail::ValidOption* entry = 0;

					/* resolve the optional-argument entry if its a full name */
					if (fullName) {
						auto it = pConfig.options.find(arg);
						if (it == pConfig.options.end()) {
							if (pDeferred.empty())
								str::BuildTo(pDeferred, L"Unknown optional argument [", arg, L"] encountered.");

							/* continue parsing, as the special entries might still occur */
							continue;
						}
						entry = &it->second;
						i = arg.size();
					}

					/* check if its one of the special entries */
					else if (!menu && pConfig.help != 0 && pConfig.help->abbreviation == arg[i]) {
						pPrintHelp = true;
						continue;
					}
					else if (!menu && pConfig.version != 0 && pConfig.version->abbreviation == arg[i]) {
						pPrintVersion = true;
						continue;
					}

					/* check if its an option abbreviation */
					else {
						auto it = pConfig.abbreviations.find(arg[i]);
						if (it == pConfig.abbreviations.end()) {
							if (pDeferred.empty())
								str::BuildTo(pDeferred, L"Unknown optional argument-abbreviation [", arg[i], L"] encountered.");

							/* continue parsing, as the special entries might still occur */
							continue;
						}
						entry = it->second;
					}

					/* check if this is a flag and mark it as seen */
					if (!entry->payload) {
						pParsed.pFlags.insert(entry->option->id);
						continue;
					}

					/* check if the payload has already been consumed */
					if (payloadUsed || (!hasPayload && pIndex >= pArgs.size())) {
						if (pDeferred.empty())
							str::BuildTo(pDeferred, L"Value [", entry->option->payload.name, L"] missing for optional argument [", entry->option->name, L"].");

						/* continue parsing, as the special entries might still occur */
						continue;
					}
					payloadUsed = true;

					/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
					auto it = pParsed.pOptions.find(entry->option->id);
					if (it == pParsed.pOptions.end())
						it = pParsed.pOptions.insert({ entry->option->id, {} }).first;
					it->second.emplace_back(arger::Value{ hasPayload ? payload : pArgs[pIndex++] });
				}

				/* check if a payload was supplied but not consumed */
				if (hasPayload && !payloadUsed && pDeferred.empty())
					str::BuildTo(pDeferred, L"Value [", payload, L"] not used by optional arguments.");
			}

		private:
			void fVerifyValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const {
				/* check if an enum was expected */
				if (std::holds_alternative<arger::Enum>(type)) {
					const arger::Enum& allowed = std::get<arger::Enum>(type);
					auto it = allowed.find(value.str());
					if (it == allowed.end())
						throw arger::ParsingException{ L"Invalid enum for argument [", name, L"] encountered." };
					value = arger::Value{ it->second };
					return;
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
					size_t index = std::min<size_t>(i, topMost->args->positionals.size() - 1);

					/* check if the default value should be used and otherwise validate the argument (default will already be validated) */
					if (pParsed.pPositional[i].str().empty() && topMost->args->positionals[index].defValue.has_value())
						pParsed.pPositional[i] = topMost->args->positionals[index].defValue.value();
					else
						fVerifyValue(topMost->args->positionals[index].name, pParsed.pPositional[i], topMost->args->positionals[index].type);
				}

				/* fill up on default values (will already be validated) */
				for (size_t i = pParsed.pPositional.size(); i < topMost->args->positionals.size(); ++i) {
					if (!topMost->args->positionals[i].defValue.has_value())
						break;
					pParsed.pPositional.push_back(topMost->args->positionals[i].defValue.value());
				}

				/* check if the minimum required number of parameters has not been reached
				*	(maximum not necessary to be checked, as it will be checked implicitly by the verification-loop) */
				if (pParsed.pPositional.size() < topMost->minimum) {
					size_t index = std::min<size_t>(topMost->args->positionals.size() - 1, pParsed.pPositional.size());
					if (pSelected == 0)
						throw arger::ParsingException{ L"Argument [", topMost->args->positionals[index].name, L"] is missing." };
					throw arger::ParsingException{ L"Argument [", topMost->args->positionals[index].name, L"] is missing for ", topMost->super->groupName, L" [", pSelected->group->name, L"]." };
				}
			}
			void fVerifyOptional() {
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);

				/* iterate over the optional arguments and verify them */
				for (auto& [name, option] : pConfig.options) {
					/* check if the current option can be used by the selected group or any of its ancestors */
					if (!detail::CheckUsage(&option, pSelected) && (option.payload ? pParsed.pOptions.contains(option.option->id) : pParsed.pFlags.contains(option.option->id))) {
						/* selected can never be null, as null would allow any usage - but static analyzer does not know this */
						if (pSelected != 0)
							throw arger::ParsingException{ L"Argument [", name, L"] not meant for ", topMost->super->groupName, L" [", pSelected->group->name, L"]." };
					}

					/* check if this is a flag, in which case nothing more needs to be checked */
					if (!option.payload)
						continue;

					/* lookup the option */
					auto it = pParsed.pOptions.find(option.option->id);
					size_t count = (it == pParsed.pOptions.end() ? 0 : it->second.size());

					/* check if the default values should be assigned (are already validated by the verifying-step) */
					if (count == 0 && !option.option->payload.defValue.empty()) {
						pParsed.pOptions.insert({ option.option->id, {} }).first->second = option.option->payload.defValue;
						continue;
					}

					/* check if the optional-argument has been found */
					if (option.minimum > count)
						throw arger::ParsingException{ L"Argument [", name, L"] is missing." };

					/* check if too many instances were found */
					if (option.maximum > 0 && count > option.maximum)
						throw arger::ParsingException{ L"Argument [", name, L"] can at most be specified ", option.maximum, " times." };

					/* verify the values themselves */
					for (size_t i = 0; i < count; ++i)
						fVerifyValue(name, it->second[i], option.option->payload.type);
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
			arger::Parsed parse(const arger::Config& config, size_t lineLength, bool menu) {
				const detail::ValidArguments* topMost = static_cast<const detail::ValidArguments*>(&pConfig);

				/* validate and pre-process the configuration */
				detail::ValidateConfig(config, pConfig, menu);

				/* extract the program name */
				detail::BaseBuilder base{ pArgs.empty() || menu ? L"" : pArgs[pIndex++], config, menu };

				/* iterate over the arguments and parse them based on the definitions */
				size_t dirtyGroup = pArgs.size();
				bool canHaveSpecialEntries = true;
				while (pIndex < pArgs.size()) {
					const std::wstring& next = pArgs[pIndex++];

					/* check if its the version or help group (only if the last entry was
					*	part of a group-selection or the top-most group is still incomplete) */
					if (menu && (canHaveSpecialEntries || topMost->incomplete)) {
						if (pConfig.help != 0 && (next.size() != 1 ? (next == pConfig.help->name) : (pConfig.help->abbreviation == next[0] && next[0] != 0))) {
							pPrintHelp = true;
							continue;
						}
						if (pConfig.version != 0 && (next.size() != 1 ? (next == pConfig.version->name) : (pConfig.version->abbreviation == next[0] && next[0] != 0))) {
							pPrintVersion = true;
							continue;
						}
					}
					canHaveSpecialEntries = false;

					/* check if its an optional argument or positional-lock */
					if (!pPositionalLocked && !next.empty() && next[0] == L'-') {
						if (next == L"--") {
							pPositionalLocked = true;
							continue;
						}

						/* check if a payload is baked into the string */
						std::wstring payload;
						size_t end = next.size();
						bool hasPayload = false;
						for (size_t i = 1; i < next.size(); ++i) {
							if (next[i] != L'=')
								continue;

							/* extract the payload and mark the actual end of the optional argument */
							hasPayload = true;
							payload = next.substr(i + 1);
							end = i;
							break;
						}

						/* parse the single long or multiple short arguments */
						size_t hypenCount = (next.size() > 2 && next[1] == L'-' ? 2 : 1);
						fParseOptional(next.substr(hypenCount, end - hypenCount), payload, (hypenCount == 2), hasPayload, menu);
						continue;
					}

					/* check if this is a group-selector */
					if (topMost->incomplete && dirtyGroup == pArgs.size()) {
						/* find the group with the matching argument-name */
						auto it = topMost->sub.find(next);

						/* check if a group has been found */
						if (it != topMost->sub.end()) {
							topMost = (pSelected = &it->second);
							canHaveSpecialEntries = true;
							continue;
						}

						/* check if an abbreviation matches */
						if (next.size() == 1) {
							auto at = topMost->abbreviations.find(next[0]);
							if (at != topMost->abbreviations.end()) {
								topMost = (pSelected = at->second);
								canHaveSpecialEntries = true;
								continue;
							}
						}
						dirtyGroup = pIndex - 1;
					}

					/* add the argument to the list of positional arguments (dont perform any validations or
					*	limit checks for now, but defer it until the help/version string have been potentially
					*	printed, in order for help to be printed without the arguments being valid) */
					pParsed.pPositional.emplace_back(arger::Value{ next });
				}

				/* check if the help or version should be printed */
				std::wstring print = (pPrintVersion ? base.buildVersionString() : L"");
				if (pPrintHelp)
					print.append(print.empty() ? L"" : L"\n\n").append(detail::HelpBuilder{ base, pConfig, pSelected, lineLength }.buildHelpString(menu));
				if (!print.empty())
					throw arger::PrintMessage{ print };

				/* verify the group selection */
				if (dirtyGroup < pArgs.size())
					throw arger::ParsingException{ L"Unknown ", topMost->groupName, L" [", pArgs[dirtyGroup], L"] encountered." };

				/* check if a deferred error can be thrown */
				if (!pDeferred.empty())
					throw arger::ParsingException{ pDeferred };

				/* verify the positional arguments */
				fVerifyPositional();

				/* verify the optional arguments */
				fVerifyOptional();

				/* setup the selected group */
				pParsed.pGroupId = (pSelected == 0 ? 0 : pSelected->group->id);

				/* validate all root/selection constraints */
				fRecCheckConstraints(topMost);

				/* validate all optional constraints */
				for (const auto& [_, option] : pConfig.options) {
					if (!(option.payload ? pParsed.pOptions.contains(option.option->id) : pParsed.pFlags.contains(option.option->id)))
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

	/* parse the arguments as standard program arguments */
	inline arger::Parsed Parse(const std::vector<std::wstring>& args, const arger::Config& config, size_t lineLength = arger::NumCharsHelp) {
		return detail::Parser{ args }.parse(config, lineLength, false);
	}

	/* parse the arguments as menu-input arguments */
	inline arger::Parsed Menu(const std::vector<std::wstring>& args, const arger::Config& config, size_t lineLength = arger::NumCharsHelp) {
		return detail::Parser{ args }.parse(config, lineLength, true);
	}
}
