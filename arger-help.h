/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2025 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"

namespace arger {
	static constexpr size_t NumCharsHelp = 100;
	static constexpr size_t NumCharsHelpLeft = 32;
	static constexpr size_t MinNumCharsRight = 8;
	static constexpr size_t IndentInformation = 4;
	static constexpr size_t AutoIndentLongText = 2;

	namespace detail {
		class BaseBuilder {
		private:
			const detail::Config& pConfig;
			std::wstring pProgram;

		public:
			BaseBuilder(const std::wstring& firstArg, const arger::Config& config) : pConfig{ detail::ConfigBurner::GetBurned(config) } {
				/* check if the first argument contains a valid program-name */
				std::wstring_view view{ firstArg };
				size_t begin = view.size();
				while (begin > 0 && view[begin - 1] != L'/' && view[begin - 1] != L'\\')
					--begin;

				/* setup the final program name (if the first argument is an empty string, the configured program name will be used) */
				pProgram = (begin == view.size() ? pConfig.program : std::wstring{ view.substr(begin) });
			}

		public:
			constexpr std::wstring buildVersionString() const {
				/* version string is guaranteed to never be empty if it can be requested to be built */
				if (pConfig.program.empty())
					return pConfig.version;
				if (pProgram.empty())
					throw arger::ConfigException{ L"Configuration must have a program name." };
				return str::wd::Build(pProgram, L' ', pConfig.version);
			}
			constexpr std::wstring buildHelpHintString() const {
				if (pConfig.special.help.name.empty())
					throw arger::ConfigException{ L"Help entry name must not be empty for help-hint string." };
				if (pConfig.program.empty())
					return str::wd::Build(L"Use '", pConfig.special.help.name, L"' for more information.");
				if (pProgram.empty())
					throw arger::ConfigException{ L"Configuration must have a program name." };
				return str::wd::Build(L"Try '", pProgram, L" --", pConfig.special.help.name, L"' for more information.");
			}
			constexpr const std::wstring& program() const {
				return pProgram;
			}
		};

		class HelpBuilder {
		private:
			struct NameCache {
				std::wstring used;
				const detail::Description* description = 0;
				const detail::ValidOption* option = 0;
				wchar_t abbreviation = 0;
			};

		private:
			std::wstring pBuffer;
			const detail::ValidArguments* pTopMost = 0;
			const detail::ValidConfig& pConfig;
			const detail::BaseBuilder& pBase;
			size_t pPosition = 0;
			size_t pNumChars = 0;
			size_t pOpenWhiteSpace = 0;
			bool pReduced = false;

		public:
			constexpr HelpBuilder(const detail::BaseBuilder& base, const detail::ValidConfig& config, const detail::ValidArguments* topMost, size_t numChars, bool reduced) : pBase{ base }, pConfig{ config }, pTopMost{ topMost }, pReduced{ reduced } {
				pNumChars = std::max(arger::NumCharsHelpLeft + arger::MinNumCharsRight, numChars);
			}

		private:
			constexpr void fAddNewLine(bool emptyLine) {
				if (pBuffer.empty())
					return;
				if (pBuffer.back() != L'\n')
					pBuffer.push_back(L'\n');
				if (emptyLine)
					pBuffer.push_back(L'\n');
				pPosition = 0;
				pOpenWhiteSpace = 0;
			}
			constexpr void fAddToken(const std::wstring& add) {
				if (pPosition > 0 && pPosition + add.size() > pNumChars) {
					pBuffer.push_back(L'\n');
					pPosition = 0;
				}

				pBuffer.append(add);
				pPosition += add.size();
				pOpenWhiteSpace = 0;
			}
			constexpr void fAddSpacedToken(const std::wstring& add) {
				if (pPosition > 0) {
					if (pPosition + 1 + add.size() > pNumChars) {
						pBuffer.push_back(L'\n');
						pPosition = 0;
					}
					else {
						pBuffer.push_back(L' ');
						++pPosition;
					}
				}
				fAddToken(add);
			}
			constexpr void fAddString(const std::wstring& add, size_t offset = 0, size_t indentAutoBreaks = 0) {
				std::wstring tokenPrint;
				bool isWhitespace = true;

				/* ensure the initial indentation is valid */
				if (offset > 0) {
					if (pPosition + pOpenWhiteSpace >= offset) {
						pBuffer.push_back(L'\n');
						pPosition = 0;
					}
					pBuffer.append(offset - pPosition, L' ');
					pPosition = offset;
					offset += indentAutoBreaks;
				}
				pOpenWhiteSpace = 0;

				/* iterate over the string and collect all ranges of whitespace, followed by printable characters, and
				*	flush them out to the output whenever the line overflows or a newline/new printable-token is encountered */
				for (size_t i = 0; i <= add.size(); ++i) {
					if (i < add.size() && !std::iswspace(add[i])) {
						isWhitespace = false;
						tokenPrint.push_back(add[i]);
						continue;
					}

					/* check if the last whitespace and printable token need to be flushed (only if printable buffer is
					*	not empty and the next character is either the end of the string or a new whitespace character) */
					if (!isWhitespace) {
						/* check if the whitespace token should be replaced with a line-break, if the printable character
						*	does not start at the beginning of the line but exceeds the line limit, and otherwise flush
						*	the whitespace (this ensures that user-placed whitespace after a new-line will be respected) */
						if ((pPosition > offset || pOpenWhiteSpace > 0) && pPosition + pOpenWhiteSpace + tokenPrint.size() > pNumChars)
							pBuffer.append(1, L'\n').append(pPosition = offset, L' ');
						else {
							pBuffer.append(pOpenWhiteSpace, L' ');
							pPosition += pOpenWhiteSpace;
						}

						/* flush the last printable token to the buffer */
						pBuffer.append(tokenPrint);
						pPosition += tokenPrint.size();
						pOpenWhiteSpace = 0;
						tokenPrint.clear();
					}
					if (i >= add.size())
						break;

					/* check if the new token is a linebreak and insert it and otherwise add the whitespace to the current token */
					isWhitespace = true;
					if (add[i] == L'\n')
						pBuffer.append(1, L'\n').append(pPosition = offset, L' ');
					else if (add[i] == L'\t')
						pOpenWhiteSpace += 4;
					else
						++pOpenWhiteSpace;
				}
			}

		private:
			constexpr std::wstring fLimitDescription(size_t minimum, size_t maximum) {
				/* check if no limit needs to be added */
				if (minimum == maximum && (minimum == 0 || minimum == 1))
					return L"";

				/* check if an upper and lower bound has been defined */
				if (minimum > 0 && maximum > 0) {
					if (minimum == maximum)
						return str::wd::Build(minimum, L'x');
					return str::wd::Build(minimum, L"...", maximum);
				}

				/* construct the one-sided limit string */
				if (minimum > 0)
					return str::wd::Build(minimum, L"...");
				return str::wd::Build(L"...", maximum);
			}
			std::wstring fDefaultDescription(const arger::Value* begin, const arger::Value* end) {
				/* construct the list of all default values */
				std::wstring temp = L"Default: ";
				for (const arger::Value* it = begin; it != end; ++it) {
					temp.append(it == begin ? L"" : L", ");

					if (it->isStr())
						temp.append(it->str());
					else if (it->isUNum())
						str::IntTo(temp, it->unum());
					else if (it->isINum())
						str::IntTo(temp, it->inum());
					else if (it->isReal())
						str::FloatTo(temp, it->real());
					else
						temp.append(it->boolean() ? L"true" : L"false");
				}
				return temp;
			}
			void fAddEnumDescription(const arger::Type& type) {
				/* check if this is an enum to be added */
				if (!std::holds_alternative<arger::Enum>(type))
					return;

				/* count the key-length */
				size_t length = 0;
				for (const auto& val : std::get<arger::Enum>(type))
					length = std::max<size_t>(length, val.name.size());

				/* add the separate keys  */
				for (const auto& val : std::get<arger::Enum>(type)) {
					fAddNewLine(false);
					const std::wstring& text = ((pReduced && !val.reduced.empty()) ? val.reduced : val.normal);
					fAddString(str::wd::Format(L" - [{:<{}}]: {}", val.name, length, text), arger::NumCharsHelpLeft, 7 + length);
				}
			}
			constexpr const wchar_t* fTypeString(const arger::Type& type) {
				if (std::holds_alternative<arger::Enum>(type))
					return L" [enum]";
				arger::Primitive actual = std::get<arger::Primitive>(type);
				if (actual == arger::Primitive::boolean)
					return L" [bool]";
				if (actual == arger::Primitive::unum)
					return L" [uint]";
				if (actual == arger::Primitive::inum)
					return L" [int]";
				if (actual == arger::Primitive::real)
					return L" [real]";
				return L"";
			}
			std::wstring fEndpointName(const detail::ValidEndpoint& endpoint, size_t index) {
				std::wstring token = (*endpoint.positionals)[index].name;
				if (index + 1 >= endpoint.positionals->size() && (endpoint.maximum == 0 || index + 1 < endpoint.maximum))
					token += L"...";
				if (index >= endpoint.minimumEffective)
					token = L"[" + token + L']';
				return token;
			}
			const std::wstring* fGetDescription(const detail::Description* desc) {
				if (desc == nullptr)
					return nullptr;
				const std::wstring& text = ((!pReduced || desc->description.reduced.empty()) ? desc->description.normal : desc->description.reduced);
				return (text.empty() ? nullptr : &text);
			}

		private:
			constexpr void fRecGroupUsage(const detail::ValidArguments* topMost) {
				if (topMost == nullptr || topMost->super == nullptr)
					return;
				fRecGroupUsage(topMost->super);
				fAddSpacedToken(topMost->group->name);
			}
			constexpr std::wstring fGroupNameHistory(const detail::ValidArguments* topMost) {
				if (topMost->super == nullptr)
					return L"";
				std::wstring parents = fGroupNameHistory(topMost->super);
				return str::wd::Build(parents, (parents.empty() ? L"[" : L" > ["), str::View{ topMost->super->groupName }.title(), L": ", topMost->group->name, L']');
			}
			constexpr void fRecInformationStrings(const detail::ValidArguments* topMost, bool top) {
				if (topMost == nullptr)
					return;
				fRecInformationStrings(topMost->super, false);

				/* iterate over the information-content and append it to the information-buffer */
				const detail::InformationList* list = (topMost->group == nullptr ? static_cast<const detail::InformationList*>(pConfig.burned) : static_cast<const detail::InformationList*>(topMost->group));
				for (const auto& info : list->information) {
					if (pReduced && info.reduced.empty())
						continue;
					if (!info.allChildren.value_or(false) && !top)
						continue;
					fAddNewLine(true);
					fAddString(info.name + L": ");
					fAddString((pReduced ? info.reduced : info.text), arger::NumCharsHelpLeft);
				}
			}
			void fAddEndpointUsage(const detail::ValidEndpoint& endpoint) {
				/* add the positional parameter for the topmost argument-object */
				for (size_t i = 0; i < endpoint.positionals->size(); ++i)
					fAddSpacedToken(fEndpointName(endpoint, i));
			}
			bool fSelectSpecial(std::map<std::wstring, NameCache>& selected) {
				bool added = false;
				if (pConfig.help != nullptr && (pTopMost->super == nullptr || pConfig.help->allChildren.value_or(true))) {
					selected.insert({ pConfig.help->name, NameCache{ L"", pConfig.help, nullptr, pConfig.help->abbreviation } });
					added = true;
				}
				if (pConfig.version != nullptr && (pTopMost->super == nullptr || pConfig.version->allChildren.value_or(true))) {
					selected.insert({ pConfig.version->name, NameCache{ L"", pConfig.version, nullptr, pConfig.version->abbreviation } });
					added = true;
				}
				return added;
			}

		private:
			bool fCheckOptionPrint(const detail::ValidOption* option) const {
				/* in program mode, check if the argument is excluded by the current selected group, based on the usage-requirements */
				if (!pConfig.burned->program.empty())
					return detail::CheckUsage(option, pTopMost);

				/* in menu-mode, print only flags, which are only available because a parent uses it,
				*	and for the special case of the root, only print it if no other groups exist */
				if (pTopMost->super == nullptr)
					return pConfig.sub.empty();
				for (const auto& user : option->users) {
					if (detail::CheckParent(user, pTopMost))
						return true;
				}
				return false;
			}
			void fBuildUsage() {
				/* add the example-usage descriptive line */
				fAddToken(pConfig.burned->program.empty() ? L"Input>" : L"Usage: ");
				if (!pConfig.burned->program.empty())
					fAddToken(pBase.program());

				/* add the already selected groups and potentially upcoming group-name */
				fRecGroupUsage(pTopMost);
				if (pTopMost->endpoints.empty())
					fAddSpacedToken(str::wd::Build(L'[', pTopMost->groupName, L']'));

				/* add all required options (must all consume a payload, as flags are never required) */
				bool hasOptionals = false;
				for (const auto& [name, option] : pConfig.options) {
					if (option.hidden)
						continue;

					/* check if options exist in theory */
					if (detail::CheckUsage(&option, pTopMost) && option.minimumEffective == 0) {
						hasOptionals = true;
						continue;
					}

					/* check if the entry can be skipped for this group */
					if (!fCheckOptionPrint(&option))
						continue;
					fAddSpacedToken(str::wd::Build(L"--", name, L"=<", option.option->payload.name, L">"));
				}
				if (hasOptionals)
					fAddSpacedToken(L"[options...]");

				/* add the hint to the arguments after the final group has been selected and add the endpoint hint */
				if (pTopMost->endpoints.empty() && pTopMost->nestedPositionals)
					fAddSpacedToken(L"[params...]");
				if (pTopMost->endpoints.size() > 1)
					fAddString(L" variation...");

				/* check if its only a single endpoint, which is printed immediately */
				if (pTopMost->endpoints.size() == 1) {
					if (!pTopMost->endpoints.front().hidden)
						fAddEndpointUsage(pTopMost->endpoints.front());
				}

				/* print all variations (only if not reduced and not hidden) */
				else if (!pReduced) {
					size_t index = 0;
					for (size_t i = 0; i < pTopMost->endpoints.size(); ++i) {
						if (pTopMost->endpoints[i].hidden)
							continue;
						if (pTopMost->endpoints.size() > 1) {
							fAddNewLine(false);
							fAddString(str::wd::Build(L"  [Variation ", ++index, L"]:"));
						}
						fAddEndpointUsage(pTopMost->endpoints[i]);
					}
				}
			}
			void fBuildOptions(bool required) {
				std::map<std::wstring, NameCache> selected;
				std::set<std::wstring> usedList;

				/* collect all of the names to be used (add the help/version options for programs and select
				*	the used-by lists, which will automatically be lexicographically sorted in itself) */
				if (!pConfig.burned->program.empty() && !required && fSelectSpecial(selected))
					usedList.insert(L"");
				for (const auto& [name, option] : pConfig.options) {
					if (option.hidden)
						continue;

					/* check if the option can be discarded based on the selection */
					if ((option.minimumEffective > 0) != required || !fCheckOptionPrint(&option))
						continue;
					std::wstring used;

					/* collect the usages (if groups still need to be selected and not all are users, and group is not hidden) */
					if (pTopMost->endpoints.empty()) {
						size_t users = 0;
						for (const auto& [name, group] : pTopMost->sub) {
							if (!group.hidden && detail::CheckUsage(&option, &group))
								++users;
						}
						if (users != pTopMost->sub.size()) for (const auto& [name, group] : pTopMost->sub) {
							if (!group.hidden && option.users.contains(&group))
								used.append(used.empty() ? L"" : L", ").append(name);
						}
					}
					selected.insert({ name, NameCache{ used, option.option, &option, option.option->abbreviation } });
					usedList.insert(used);
				}

				/* iterate over the used-lists and optionals and print them (will automatically be
				*	lexicographically sorted by used-list, with empty first, and then by option name) */
				for (const std::wstring& used : usedList) {
					/* print the used-header */
					fAddNewLine(true);
					std::wstring temp = str::wd::Build(required ? L"Required" : L"Optional");
					if (used.empty())
						temp.push_back(L':');
					else
						str::BuildTo(temp, L" for [", used, L"]: ");
					fAddString(temp);

					/* print the optionals, which are part of the used list */
					for (const auto& [name, cache] : selected) {
						if (cache.used != used)
							continue;
						fAddNewLine(false);

						/* add the abbreviation and name */
						std::wstring temp = L"  ";
						if (cache.abbreviation != 0)
							temp.append(1, L'-').append(1, cache.abbreviation).append(L", ");
						temp.append(L"--").append(name);
						const detail::ValidOption* option = cache.option;

						/* add the payload (append white space for visual separation) */
						if (option != nullptr && option->payload)
							temp.append(L"=<").append(option->option->payload.name).append(fTypeString(option->option->payload.type)).append(1, L'>');
						temp.append(L"    ");
						fAddString(temp);

						/* add the custom usage-limits and default values */
						temp.clear();
						if (option != nullptr) {
							std::wstring limit = fLimitDescription(option->minimumActual, option->maximum > 1 ? option->maximum : 0), defDesc;
							if (!option->option->payload.defValue.empty())
								defDesc = fDefaultDescription(&option->option->payload.defValue.front(), &option->option->payload.defValue.back() + 1);
							if (!limit.empty() || !defDesc.empty())
								temp = str::wd::Build(L'[', limit, ((limit.empty() || defDesc.empty()) ? L"" : L"; "), defDesc, L']');
						}

						/* write the description out and check if the enum descriptions need to be written out as well */
						if (const std::wstring* text = fGetDescription(cache.description); text != nullptr)
							temp.append((temp.empty() ? 0 : 1), L' ').append(*text);
						fAddString(temp, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
						if (option != nullptr)
							fAddEnumDescription(option->option->payload.type);
					}
				}
			}
			void fBuildGroups() {
				/* collect the list of all names to be used (add the help and version entries for menus) */
				std::map<std::wstring, NameCache> selected;
				if (pConfig.burned->program.empty())
					fSelectSpecial(selected);
				if (pTopMost->endpoints.empty()) for (const auto& [name, group] : pTopMost->sub) {
					if (!group.hidden)
						selected.insert({ name, NameCache{ L"", group.group, nullptr, group.group->abbreviation } });
				}

				/* check if anything actually needs to be printed */
				if (selected.empty())
					return;

				/* print the header */
				fAddNewLine(true);
				if (pTopMost->endpoints.empty())
					fAddString(str::wd::Build(L"Defined for [", pTopMost->groupName, L"]:"));
				else
					fAddString(str::wd::Build(L"Optional Keywords:"));

				/* print all of the selected entries */
				for (const auto& [name, cache] : selected) {
					fAddNewLine(false);
					fAddString(str::wd::Build(L"  ", name));
					if (cache.abbreviation != 0)
						fAddString(str::wd::Build(L", ", cache.abbreviation));
					if (const std::wstring* text = fGetDescription(cache.description); text != nullptr)
						fAddString(*text, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
				}
			}
			void fBuildEndpoints() {
				if (pTopMost->endpoints.empty())
					return;

				/* add the separate endpoints */
				size_t index = 0;
				for (size_t i = 0; i < pTopMost->endpoints.size(); ++i) {
					const detail::ValidEndpoint& endpoint = pTopMost->endpoints[i];
					if (endpoint.hidden)
						continue;

					/* add the heading (with description for the variations) */
					fAddNewLine(true);
					if (pTopMost->endpoints.size() == 1)
						fAddString(L"Positional Arguments:");
					else {
						fAddString(str::wd::Build(L"Variation ", ++index, L':'));
						fAddEndpointUsage(pTopMost->endpoints[i]);
						if (const std::wstring* text = fGetDescription(endpoint.description); text != nullptr)
							fAddString(*text, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
					}

					/* add the positional arguments descriptions (will automatically be sorted by position) */
					for (size_t j = 0; j < endpoint.positionals->size(); ++j) {
						const detail::Positional& positional = (*endpoint.positionals)[j];
						fAddNewLine(false);

						/* add the name and corresponding type and description (add visual separation in the end) */
						fAddString(str::wd::Build("  ", positional.name, fTypeString(positional.type), L"    "));
						std::wstring temp, limit, defDesc;

						/* add the limit and default values and write the value out */
						if (j + 1 >= endpoint.positionals->size())
							limit = fLimitDescription((endpoint.minimumEffective > j ? endpoint.minimumEffective - j : 0), (endpoint.maximum > j ? endpoint.maximum - j : 0));
						if (positional.defValue.has_value())
							defDesc = fDefaultDescription(&positional.defValue.value(), &positional.defValue.value() + 1);
						if (!limit.empty() || !defDesc.empty())
							temp = str::wd::Build(L'[', limit, ((limit.empty() || defDesc.empty()) ? L"" : L"; "), defDesc, L']');

						/* add the description and append the enum value description */
						if (const std::wstring* text = fGetDescription(&positional); text != nullptr)
							temp.append((temp.empty() ? 0 : 1), L' ').append(*text);
						fAddString(temp, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
						fAddEnumDescription(positional.type);
					}
				}
			}
			void fBuildGroupDescription(const detail::ValidArguments* topMost) {
				/* add the description of all parents */
				if (topMost == nullptr || topMost->super == nullptr)
					return;
				if (!pReduced)
					fBuildGroupDescription(topMost->super);

				/* select the proper description to be used */
				const std::wstring* desc = fGetDescription(topMost->group);
				if (desc == nullptr)
					return;

				/* add the newline, description header, and description itself */
				fAddNewLine(true);
				if (!pReduced)
					fAddString(fGroupNameHistory(topMost));
				fAddString(*desc, arger::IndentInformation);
			}

		public:
			std::wstring buildHelpString() {
				/* update top-most to the first not-hidden entry (root config can never be hidden) */
				while (pTopMost->super != nullptr && pTopMost->hidden)
					pTopMost = pTopMost->super;

				/* add the example-usage descriptive line */
				fBuildUsage();

				/* add the program description */
				if (const std::wstring* text = fGetDescription(pConfig.burned); text != nullptr && (!pReduced || pTopMost->super == nullptr)) {
					fAddNewLine(true);
					fAddString(*text, arger::IndentInformation);
				}

				/* add the group description, the description of all sub-groups, the description of the endpoints,
				*	and the required and optional option descriptions (will automatically be sorted lexicographically) */
				fBuildGroupDescription(pTopMost);
				fBuildGroups();
				fBuildEndpoints();
				fBuildOptions(true);
				fBuildOptions(false);

				/* add all of the information descriptions for the selected group */
				fRecInformationStrings(pTopMost, true);

				/* return the constructed help-string */
				std::wstring out;
				std::swap(out, pBuffer);
				return out;
			}
		};
	}

	/* construct help-hint suggesting to use the help entry (for example 'try --help', or 'use help') */
	inline std::wstring HelpHint(const std::vector<std::wstring>& args, const arger::Config& config) {
		return detail::BaseBuilder{ (args.empty() ? L"" : args[0]), config }.buildHelpHintString();
	}
}
