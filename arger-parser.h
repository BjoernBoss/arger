/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"
#include "arger-help.h"

namespace arger {
	namespace detail {
		enum class PrintOption : uint8_t {
			none,
			reduced,
			full
		};

		class Parser {
		private:
			const std::vector<std::wstring>& pArgs;
			detail::ValidConfig pConfig;
			const detail::ValidArguments* pTopMost = nullptr;
			arger::Parsed pParsed;
			std::wstring pDeferred;
			size_t pIndex = 0;
			detail::PrintOption pPrintHelp = detail::PrintOption::none;
			bool pPrintVersion = false;
			bool pPositionalLocked = false;

		public:
			Parser(const std::vector<std::wstring>& args) : pArgs{ args } {}

		private:
			void fParseOptional(const std::wstring& arg, const std::wstring& payload, bool fullName, bool hasPayload) {
				bool payloadUsed = false;

				/* check if it could be the help or version entry (only relevant for programs) */
				size_t i = 0;
				if (fullName && !pConfig.burned->program.empty()) {
					if (pConfig.help != nullptr && pConfig.help->name == arg) {
						pPrintHelp = detail::PrintOption::full;
						i = arg.size();
					}
					else if (pConfig.version != nullptr && pConfig.version->name == arg) {
						pPrintVersion = true;
						i = arg.size();
					}
				}

				/* iterate over the list of optional abbreviations/single full-name and process them */
				for (; i < arg.size(); ++i) {
					detail::ValidOption* entry = nullptr;

					/* resolve the optional-argument entry if its a full name */
					if (fullName) {
						auto it = pConfig.options.find(arg);
						if (it == pConfig.options.end()) {
							if (pDeferred.empty())
								str::BuildTo(pDeferred, L"Unknown option [", arg, L"] encountered.");

							/* continue parsing, as the special entries might still occur */
							continue;
						}
						entry = &it->second;
						i = arg.size();
					}

					/* check if its one of the special entries (only relevant for programs) */
					else if (!pConfig.burned->program.empty() && pConfig.help != nullptr && pConfig.help->abbreviation == arg[i]) {
						if (pPrintHelp != detail::PrintOption::full)
							pPrintHelp = detail::PrintOption::reduced;
						continue;
					}
					else if (!pConfig.burned->program.empty() && pConfig.version != nullptr && pConfig.version->abbreviation == arg[i]) {
						pPrintVersion = true;
						continue;
					}

					/* check if its an option abbreviation */
					else {
						auto it = pConfig.abbreviations.find(arg[i]);
						if (it == pConfig.abbreviations.end()) {
							if (pDeferred.empty())
								str::BuildTo(pDeferred, L"Unknown option abbreviation [", arg[i], L"] encountered.");

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

					/* allocate the entry as found option */
					auto it = pParsed.pOptions.find(entry->option->id);
					if (it == pParsed.pOptions.end())
						it = pParsed.pOptions.insert({ entry->option->id, { {}, 0 } }).first;

					/* check if the payload has already been consumed (if so, mark it as forgotten;
					*	will be handled once ensured that the flag is relevant for this group) */
					if (payloadUsed || (!hasPayload && pIndex >= pArgs.size())) {
						it->second.first.clear();

						/* continue parsing, as the special entries might still occur */
						continue;
					}
					payloadUsed = true;

					/* write the value as raw string into the buffer (dont perform any validations or limit checks for now) */
					it->second.first.emplace_back(arger::Value{ hasPayload ? payload : pArgs[pIndex++] });
				}

				/* check if a payload was supplied but not consumed */
				if (hasPayload && !payloadUsed && pDeferred.empty())
					str::BuildTo(pDeferred, L"Value [", payload, L"] not used by option.");
			}

		private:
			void fUnpackDefValue(arger::Value& value, const arger::Type& type) const {
				/* check if the type is an enum, in which case the type needs to be unpacked (as default enums are stored as strings, for printing) */
				if (!std::holds_alternative<arger::Enum>(type))
					return;

				/* cannot fail, as default values have already been verified to be part of the type */
				const arger::Enum& list = std::get<arger::Enum>(type);
				size_t id = std::find_if(list.begin(), list.end(), [&](const arger::EnumEntry& e) { return e.name == value.str(); })->id;
				value = arger::Value{ detail::EnumId{.id = id } };
			}
			void fVerifyValue(const std::wstring& name, arger::Value& value, const arger::Type& type, bool option) const {
				/* check if an enum was expected */
				if (std::holds_alternative<arger::Enum>(type)) {
					const arger::Enum& list = std::get<arger::Enum>(type);

					/* resolve the id of the string */
					auto it = std::find_if(list.begin(), list.end(), [&](const arger::EnumEntry& e) { return e.name == value.str(); });
					if (it == list.end())
						throw arger::ParsingException{ L"Invalid enum for ", (option ? L"option" : L"argument"), L" [", name, L"] encountered." };

					/* assign the new enum-id */
					value = arger::Value{ detail::EnumId{.id = it->id } };
					return;
				}

				/* validate the expected type and found value */
				switch (std::get<arger::Primitive>(type)) {
				case arger::Primitive::inum: {
					auto [num, len, res] = str::SiParseNum<int64_t>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid signed integer for ", (option ? L"option" : L"argument"), L" [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::unum: {
					auto [num, len, res] = str::SiParseNum<uint64_t>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid unsigned integer for ", (option ? L"option" : L"argument"), L" [", name, L"] encountered." };
					value = arger::Value{ num };
					break;
				}
				case arger::Primitive::real: {
					auto [num, len, res] = str::SiParseNum<double>(value.str(), { .prefix = str::PrefixMode::overwrite, .scale = str::SiScaleMode::detect });
					if (res != str::NumResult::valid || len != value.str().size())
						throw arger::ParsingException{ L"Invalid real for ", (option ? L"option" : L"argument"), L" [", name, L"] encountered." };
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
					throw arger::ParsingException{ L"Invalid boolean for ", (option ? L"option" : L"argument"), L" [", name, L"] encountered." };
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

				/* select the endpoint to be used (this ensures that for too few arguments, the first one is picked, too
				*	many arguments, the last one is picked, and for in-between, the one next in line is picked) */
				const detail::ValidEndpoint* endpoint = &pTopMost->endpoints[0];
				for (size_t i = 1; i < pTopMost->endpoints.size(); ++i) {
					if (pTopMost->endpoints[i].minimumEffective <= pParsed.pPositional.size()) {
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
						if (pTopMost->super == nullptr)
							throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered." };
						throw arger::ParsingException{ L"Unrecognized argument [", pParsed.pPositional[i].str(), L"] encountered for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };
					}
					size_t index = std::min<size_t>(i, endpoint->positionals->size() - 1);

					/* unpack and verify the value */
					fVerifyValue((*endpoint->positionals)[index].name, pParsed.pPositional[i], (*endpoint->positionals)[index].type, false);
				}

				/* check if the minimum required number of parameters has not been reached (maximum not
				*	necessary to be checked, as it was checked implicitly by the verification-loop) */
				if (pParsed.pPositional.size() < endpoint->minimumEffective) {
					size_t index = std::min<size_t>(endpoint->positionals->size() - 1, pParsed.pPositional.size());
					if (pTopMost->super == nullptr)
						throw arger::ParsingException{ L"Argument [", (*endpoint->positionals)[index].name, L"] is missing." };
					throw arger::ParsingException{ L"Argument [", (*endpoint->positionals)[index].name, L"] is missing for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };
				}

				/* update the actual supplied-counter of the positionals (before adding the default-values) */
				pParsed.pSuppliedPositionals = pParsed.pPositional.size();

				/* fill the positional arguments up on default values (cannot violate maximum,
				*	as maximum will always at least fit the number of defined positionals) */
				size_t minimum = std::max<size_t>(endpoint->positionals->size(), endpoint->minimumActual);
				for (size_t i = pParsed.pPositional.size(); i < minimum; ++i) {
					size_t index = std::min<size_t>(endpoint->positionals->size() - 1, i);
					if (!(*endpoint->positionals)[index].defValue.has_value())
						break;
					pParsed.pPositional.push_back(*(*endpoint->positionals)[index].defValue);
					fUnpackDefValue(pParsed.pPositional.back(), (*endpoint->positionals)[index].type);
				}
				return endpoint;
			}
			void fVerifyOptional() {
				/* iterate over the options and verify them */
				for (auto& [name, option] : pConfig.options) {
					/* check if the current option can be used by the selected group or any of its ancestors (cannot defererence
					*	nullptr, as the root top-most, with no group, will always be allowed to use an option) */
					if (!detail::CheckUsage(&option, pTopMost) && (option.payload ? pParsed.pOptions.contains(option.option->id) : pParsed.pFlags.contains(option.option->id)))
						throw arger::ParsingException{ L"Option [", name, L"] not meant for ", pTopMost->super->groupName, L" [", pTopMost->group->name, L"]." };

					/* check if this is a flag, in which case nothing more needs to be checked */
					if (!option.payload)
						continue;

					/* lookup the option */
					auto it = pParsed.pOptions.find(option.option->id);
					size_t count = (it == pParsed.pOptions.end() ? 0 : it->second.first.size());

					/* check if the payload was forgotten - at least once, in which case the entry will exist but without any values */
					if (it != pParsed.pOptions.end() && count == 0)
						throw arger::ParsingException{ L"Payload [", option.option->payload.name, L"] of option [", option.option->name, L"] missing." };

					/* check if default values should be assigned (limits are already ensured to be valid) */
					if (count < option.option->payload.defValue.size()) {
						if (it == pParsed.pOptions.end())
							it = pParsed.pOptions.insert({ option.option->id, { {}, 0 } }).first;
						it->second.first.insert(it->second.first.end(), option.option->payload.defValue.begin() + count, option.option->payload.defValue.end());
						for (size_t i = count; i < it->second.first.size(); ++i)
							fUnpackDefValue(it->second.first[i], option.option->payload.type);
					}

					/* validate the limits of the supplied options */
					else if (option.minimumActual > count)
						throw arger::ParsingException{ L"Option [", name, L"] is missing." };
					else if (option.maximum > 0 && count > option.maximum)
						throw arger::ParsingException{ L"Option [", name, L"] can at most be specified ", option.maximum, L" times." };

					/* verify and unpack the values themselves */
					for (size_t i = 0; i < count; ++i)
						fVerifyValue(name, it->second.first[i], option.option->payload.type, true);

					/* update the supplied count */
					if (it != pParsed.pOptions.end())
						it->second.second = count;
				}
			}
			void fCheckConstraints(const std::vector<arger::Checker>& constraints) {
				for (const auto& fn : constraints) {
					if (std::wstring err = fn(pParsed); !err.empty())
						throw arger::ParsingException{ err };
				}
			}
			void fRecCheckConstraints(const detail::ValidArguments* args) {
				if (args == nullptr)
					return;
				fRecCheckConstraints(args->super);
				fCheckConstraints(*args->constraints);
			}

		public:
			arger::Parsed parse(const arger::Config& config, size_t lineLength) {
				pTopMost = static_cast<const detail::ValidArguments*>(&pConfig);

				/* validate and pre-process the configuration (valid-config must not outlive the config) */
				detail::ValidateConfig(config, pConfig);

				/* extract the program name (no argument needed for menus) */
				detail::BaseBuilder base{ ((pArgs.empty() || pConfig.burned->program.empty()) ? L"" : pArgs[pIndex++]), config };

				/* iterate over the arguments and parse them based on the definitions */
				while (pIndex < pArgs.size()) {
					const std::wstring& next = pArgs[pIndex++];

					/* check if its the version or help group (only relevant for menus and only if no positional arguments have yet been pushed) */
					if (pConfig.burned->program.empty() && pParsed.pPositional.empty()) {
						if (pConfig.help != nullptr && (next.size() != 1 ? (next == pConfig.help->name) : (pConfig.help->abbreviation == next[0] && next[0] != 0))) {
							pPrintHelp = ((next.size() > 1 || pPrintHelp == detail::PrintOption::full) ? detail::PrintOption::full : detail::PrintOption::reduced);
							continue;
						}
						if (pConfig.version != nullptr && (next.size() != 1 ? (next == pConfig.version->name) : (pConfig.version->abbreviation == next[0] && next[0] != 0))) {
							pPrintVersion = true;
							continue;
						}
					}

					/* check if its an options or positional-lock */
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

							/* extract the payload and mark the actual end of the options */
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
						const detail::ValidArguments* _next = nullptr;
						if (auto it = pTopMost->sub.find(next); it != pTopMost->sub.end())
							_next = &it->second;
						else if (auto it = pTopMost->abbreviations.find(next.size() == 1 ? next[0] : L'\0'); it != pTopMost->abbreviations.end())
							_next = it->second;

						/* check if a matching group has been found */
						if (_next != nullptr) {
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
				if (pPrintHelp != detail::PrintOption::none) {
					bool reduced = (pPrintHelp == detail::PrintOption::reduced && pConfig.help != nullptr && pConfig.help->reducible);
					print.append(print.empty() ? L"" : L"\n\n").append(detail::HelpBuilder{ base, pConfig, pTopMost, lineLength, reduced }.buildHelpString());
				}
				if (!print.empty())
					throw arger::PrintMessage{ print };

				/* check if a deferred error can be thrown */
				if (!pDeferred.empty())
					throw arger::ParsingException{ pDeferred };

				/* verify the positional arguments */
				const detail::ValidEndpoint* endpoint = fVerifyPositional();
				pParsed.pEndpoint = endpoint->id;

				/* verify the options */
				fVerifyOptional();

				/* validate all root/selection constraints */
				fRecCheckConstraints(pTopMost);

				/* validate the endpoint constraints */
				if (endpoint != nullptr && endpoint->constraints != nullptr)
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
