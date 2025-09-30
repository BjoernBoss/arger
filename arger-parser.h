/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
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
			const detail::ValidArguments* pTopMost = 0;
			arger::Parsed pParsed;
			std::wstring pDeferred;
			size_t pIndex = 0;
			bool pPrintHelp = false;
			bool pPrintVersion = false;
			bool pPositionalLocked = false;

		public:
			Parser(const std::vector<std::wstring>& args) : pArgs{ args } {}

		private:
			void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, bool hasPayload) {
				bool payloadUsed = false;

				/* check if it could be the help or version entry (only relevant for programs) */
				size_t i = 0;
				if (fullName && !pConfig.config->program.empty()) {
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

					/* check if its one of the special entries (only relevant for programs) */
					else if (!pConfig.config->program.empty() && pConfig.help != 0 && pConfig.help->abbreviation == arg[i]) {
						pPrintHelp = true;
						continue;
					}
					else if (!pConfig.config->program.empty() && pConfig.version != 0 && pConfig.version->abbreviation == arg[i]) {
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
			void fResolveEnum(const std::wstring& name, arger::Value& value, const arger::Enum& allowed) const {
				/* resolve the id of the string */
				auto it = std::find_if(allowed.begin(), allowed.end(), [&](const arger::EnumEntry& e) { return e.name == value.str(); });
				if (it == allowed.end())
					throw arger::ParsingException{ L"Invalid enum for argument [", name, L"] encountered." };

				/* assign the new enum-id */
				value = arger::Value{ detail::EnumId{ it->id } };
			}
			void fUnpackDefValue(arger::Value& value, const arger::Type& type) const {
				/* check if the type is an enum, in which case the type needs to be unpacked (as default enums are stored as
				*	strings, for printing; cannot fail, as default values have already been verified to be part of the type) */
				if (std::holds_alternative<arger::Enum>(type))
					fResolveEnum(L"", value, std::get<arger::Enum>(type));
			}
			void fVerifyValue(const std::wstring& name, arger::Value& value, const arger::Type& type) const {
				/* check if an enum was expected */
				if (std::holds_alternative<arger::Enum>(type)) {
					fResolveEnum(name, value, std::get<arger::Enum>(type));
					return;
				}

				/* validate the expected type and found value */
				switch (std::get<arger::Primitive>(type)) {
				case arger::Primitive::inum: {
					auto [num, len, res] = str::SiParseNum<int64_t>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid signed integer for argument [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::unum: {
					auto [num, len, res] = str::SiParseNum<uint64_t>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid unsigned integer for argument [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::real: {
					auto [num, len, res] = str::SiParseNum<double>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
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
			constexpr const detail::ValidEndpoint* fVerifyPositional() {
				/* check if the topmost object is still incomplete */
				if (pTopMost->endpoints.empty())
					throw arger::ParsingException{ str::View{ pTopMost->groupName }.title(), L" missing." };

				/* select the endpoint to be used (this ensures that for too few arguments, the first one is picked, too many
				*	arguments, the last one is picked, and for in-between, the one next in line is picked) */
				const detail::ValidEndpoint* endpoint = &pTopMost->endpoints[0];
				for (size_t i = 1; i < pTopMost->endpoints.size(); ++i) {
					if (pTopMost->endpoints[i].minimum <= pParsed.pPositional.size()) {
						endpoint = &pTopMost->endpoints[i];
						continue;
					}
					if (endpoint->maximum < pParsed.pPositional.size())
						endpoint = &pTopMost->endpoints[i];
					break;
				}

				/* validate the requirements for the positional arguments and parse their values */
				for (size_t i = 0; i < pParsed.pPositional.size(); ++i) {
					/* check if the argument is out of range */
					if (endpoint->positionals->empty() || (endpoint->maximum > 0 && i >= endpoint->maximum)) {
						if (pTopMost->super == 0)
							throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered." };
						throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };
					}
					size_t index = std::min<size_t>(i, endpoint->positionals->size() - 1);

					/* check if the value is empty and the default value should be used and otherwise unpack the value */
					if (pParsed.pPositional[i].str().empty() && (*endpoint->positionals)[index].defValue.has_value()) {
						pParsed.pPositional[i] = (*endpoint->positionals)[index].defValue.value();
						fUnpackDefValue(pParsed.pPositional[i], (*endpoint->positionals)[index].type);
					}
					else
						fVerifyValue((*endpoint->positionals)[index].name, pParsed.pPositional[i], (*endpoint->positionals)[index].type);
				}

				/* fill the positional arguments up on default values (ensures later verification and makes unpacking
				*	easier - cannot violate maximum, as maximum will always at least fit the number of defined positionals) */
				for (size_t i = pParsed.pPositional.size(); i < endpoint->positionals->size(); ++i) {
					if (!(*endpoint->positionals)[i].defValue.has_value())
						break;
					pParsed.pPositional.push_back((*endpoint->positionals)[i].defValue.value());
					fUnpackDefValue(pParsed.pPositional.back(), (*endpoint->positionals)[i].type);
				}

				/* check if the minimum required number of parameters has not been reached
				*	(maximum not necessary to be checked, as it will be checked implicitly by the verification-loop) */
				if (pParsed.pPositional.size() < endpoint->minimum) {
					size_t index = std::min<size_t>(endpoint->positionals->size() - 1, pParsed.pPositional.size());
					if (pTopMost->super == 0)
						throw arger::ParsingException{ L"Argument [", (*endpoint->positionals)[index].name, L"] is missing." };
					throw arger::ParsingException{ L"Argument [", (*endpoint->positionals)[index].name, L"] is missing for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };
				}
				return endpoint;
			}
			void fVerifyOptional() {
				/* iterate over the optional arguments and verify them */
				for (auto& [name, option] : pConfig.options) {
					/* check if the current option can be used by the selected group or any of its ancestors */
					if (!detail::CheckUsage(&option, pTopMost) && (option.payload ? pParsed.pOptions.contains(option.option->id) : pParsed.pFlags.contains(option.option->id)))
						throw arger::ParsingException{ L"Argument [", name, L"] not meant for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };

					/* check if this is a flag, in which case nothing more needs to be checked */
					if (!option.payload)
						continue;

					/* lookup the option */
					auto it = pParsed.pOptions.find(option.option->id);
					size_t count = (it == pParsed.pOptions.end() ? 0 : it->second.size());

					/* check if the default values should be assigned (limits are already ensured to be valid) */
					if (count == 0 && !option.option->payload.defValue.empty()) {
						it = pParsed.pOptions.insert({ option.option->id, option.option->payload.defValue }).first;
						for (size_t i = 0; i < it->second.size(); ++i)
							fUnpackDefValue(it->second[i], option.option->payload.type);
						continue;
					}

					/* check if the optional-argument has been found */
					if (option.minimum > count)
						throw arger::ParsingException{ L"Argument [", name, L"] is missing." };

					/* check if too many instances were found */
					if (option.maximum > 0 && count > option.maximum)
						throw arger::ParsingException{ L"Argument [", name, L"] can at most be specified ", option.maximum, " times." };

					/* verify and unpack the values themselves */
					for (size_t i = 0; i < count; ++i)
						fVerifyValue(name, it->second[i], option.option->payload.type);
				}
			}
			void fCheckConstraints(const std::vector<arger::Checker>& constraints) {
				for (const auto& fn : constraints) {
					if (std::wstring err = fn(pParsed); !err.empty())
						throw arger::ParsingException{ err };
				}
			}
			void fRecCheckConstraints(const detail::ValidArguments* args) {
				if (args == 0)
					return;
				fRecCheckConstraints(args->super);
				fCheckConstraints(*args->constraints);
			}

		public:
			arger::Parsed parse(const arger::Config& config, size_t lineLength) {
				pTopMost = static_cast<const detail::ValidArguments*>(&pConfig);

				/* validate and pre-process the configuration */
				detail::ValidateConfig(config, pConfig);

				/* extract the program name (no argument needed for menus) */
				detail::BaseBuilder base{ ((pArgs.empty() || pConfig.config->program.empty()) ? L"" : pArgs[pIndex++]), config };

				/* iterate over the arguments and parse them based on the definitions */
				while (pIndex < pArgs.size()) {
					const std::wstring& next = pArgs[pIndex++];

					/* check if its the version or help group (only relevant for menus and only if no positional arguments have yet been pushed) */
					if (pConfig.config->program.empty() && pParsed.pPositional.empty()) {
						if (pConfig.help != 0 && (next.size() != 1 ? (next == pConfig.help->name) : (pConfig.help->abbreviation == next[0] && next[0] != 0))) {
							pPrintHelp = true;
							continue;
						}
						if (pConfig.version != 0 && (next.size() != 1 ? (next == pConfig.version->name) : (pConfig.version->abbreviation == next[0] && next[0] != 0))) {
							pPrintVersion = true;
							continue;
						}
					}

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
						fParseOptional(next.substr(hypenCount, end - hypenCount), payload, (hypenCount == 2), hasPayload);
						continue;
					}

					/* check if this is a group-selector */
					if (pTopMost->endpoints.empty()) {
						/* find the group with the matching argument-name */
						const detail::ValidArguments* _next = 0;
						if (auto it = pTopMost->sub.find(next); it != pTopMost->sub.end())
							_next = &it->second;
						else if (auto it = pTopMost->abbreviations.find(next.size() == 1 ? next[0] : L'\0'); it != pTopMost->abbreviations.end())
							_next = it->second;

						/* check if a matching group has been found */
						if (_next != 0) {
							pTopMost = _next;
							pParsed.pGroupIds.push_back(pTopMost->group->id);
							continue;
						}

						/* setup the failed match */
						if (pDeferred.empty())
							str::BuildTo(pDeferred, L"Unknown ", pTopMost->groupName, L" [", next, L"] encountered.");
						continue;
					}

					/* add the argument to the list of positional arguments (dont perform any validations or
					*	limit checks for now, but defer it until the help/version string have been potentially
					*	printed, in order for help to be printed without the arguments being valid) */
					pParsed.pPositional.emplace_back(arger::Value{ next });
				}

				/* check if the help or version should be printed */
				std::wstring print = (pPrintVersion ? base.buildVersionString() : L"");
				if (pPrintHelp)
					print.append(print.empty() ? L"" : L"\n\n").append(detail::HelpBuilder{ base, pConfig, pTopMost, lineLength }.buildHelpString());
				if (!print.empty())
					throw arger::PrintMessage{ print };

				/* check if a deferred error can be thrown */
				if (!pDeferred.empty())
					throw arger::ParsingException{ pDeferred };

				/* verify the positional arguments */
				const detail::ValidEndpoint* endpoint = fVerifyPositional();
				pParsed.pEndpoint = endpoint->id;

				/* verify the optional arguments */
				fVerifyOptional();

				/* validate all root/selection constraints */
				fRecCheckConstraints(pTopMost);

				/* validate the endpoint constraints */
				if (endpoint != 0 && endpoint->constraints != 0)
					fCheckConstraints(*endpoint->constraints);

				/* validate all optional constraints */
				for (const auto& [_, option] : pConfig.options) {
					if ((option.payload ? pParsed.pOptions.contains(option.option->id) : pParsed.pFlags.contains(option.option->id)))
						fCheckConstraints(option.option->constraints);
				}

				/* return the parsed structure */
				arger::Parsed out;
				std::swap(out, pParsed);
				return out;
			}
		};
	}

	/* parse the arguments as standard program or menu-input arguments */
	inline arger::Parsed Parse(const std::vector<std::wstring>& args, const arger::Config& config, size_t lineLength = arger::NumCharsHelp) {
		return detail::Parser{ args }.parse(config, lineLength);
	}
}
