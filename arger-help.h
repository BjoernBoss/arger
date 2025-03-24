/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"

namespace arger {
	static constexpr size_t NumCharsHelp = 100;

	namespace detail {
		static constexpr size_t NumCharsHelpLeft = 32;

		class BaseBuilder {
		private:
			const arger::Config& pConfig;
			std::wstring pProgram;

		public:
			constexpr BaseBuilder(const std::wstring& firstArg, const arger::Config& config, bool menu) : pConfig{ config } {
				if (menu)
					return;
				if (config.program.empty())
					throw arger::ConfigException{ L"Program must not be empty." };

				/* check if the first argument contains a valid program-name */
				std::wstring_view view{ firstArg };
				size_t begin = view.size();
				while (begin > 0 && view[begin - 1] != L'/' && view[begin - 1] != L'\\')
					--begin;

				/* setup the final program name */
				pProgram = (begin == view.size() ? config.program : std::wstring{ view.substr(begin) });
			}

		public:
			constexpr std::wstring buildVersionString() const {
				return str::wd::Build(pProgram, L" Version [", pConfig.version, L']');
			}
			constexpr std::wstring buildHelpHintString() const {
				return str::wd::Build(L"Try '", pProgram, L" --help' for more information.");
			}
			constexpr const std::wstring& program() const {
				return pProgram;
			}
		};

		class HelpBuilder {
		private:
			struct NameCache {
				const std::wstring* description = 0;
				const detail::ValidOption* option = 0;
				wchar_t abbreviation = 0;
			};

		private:
			std::wstring pBuffer;
			size_t pPosition = 0;
			const detail::ValidArguments* pTopMost = 0;
			const detail::ValidConfig& pConfig;
			const detail::BaseBuilder& pBase;
			size_t pNumChars = 0;

		public:
			constexpr HelpBuilder(const detail::BaseBuilder& base, const detail::ValidConfig& config, const detail::ValidArguments* topMost, size_t numChars) : pBase{ base }, pConfig{ config }, pTopMost{ topMost } {
				pNumChars = std::max(detail::NumCharsHelpLeft + 8, numChars);
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
			}
			constexpr void fAddToken(const std::wstring& add) {
				if (pPosition > 0 && pPosition + add.size() > pNumChars) {
					pBuffer.push_back(L'\n');
					pPosition = 0;
				}

				pBuffer.append(add);
				pPosition += add.size();
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
				std::wstring tokenWhite, tokenPrint;
				bool isWhitespace = true;

				/* ensure the initial indentation is valid */
				if (offset > 0) {
					if (pPosition >= offset) {
						pBuffer.push_back(L'\n');
						pPosition = 0;
					}
					pBuffer.append(offset - pPosition, L' ');
					pPosition = offset;
					offset += indentAutoBreaks;
				}

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
						if ((pPosition > offset || tokenWhite.size() > 0) && pPosition + tokenWhite.size() + tokenPrint.size() > pNumChars)
							pBuffer.append(1, L'\n').append(pPosition = offset, L' ');
						else {
							pBuffer.append(tokenWhite);
							pPosition += tokenWhite.size();
						}

						/* flush the last printable token to the buffer */
						pBuffer.append(tokenPrint);
						pPosition += tokenPrint.size();
						tokenWhite.clear();
						tokenPrint.clear();
					}
					if (i >= add.size())
						break;

					/* check if the new token is a linebreak and insert it and otherwise add the whitespace to the current token */
					isWhitespace = true;
					if (add[i] == L'\n')
						pBuffer.append(1, L'\n').append(pPosition = offset, L' ');
					else
						tokenWhite.push_back(add[i]);
				}
			}

		private:
			constexpr std::wstring fLimitString(size_t minimum, size_t maximum) {
				/* check if no limit needs to be added */
				if (minimum == 0 && maximum == 0)
					return L"";

				/* check if an upper and lower bound has been defined */
				if (minimum > 0 && maximum > 0) {
					if (minimum == maximum)
						return str::wd::Build(L" [", minimum, L"x]");
					return str::wd::Build(L" [", minimum, L" <= _ <= ", maximum, L']');
				}

				/* construct the one-sided limit string */
				if (minimum > 0)
					return str::wd::Build(L" [>= ", minimum, L']');
				return str::wd::Build(L" [<= ", maximum, L']');
			}
			void fEnumDescription(const arger::Type& type) {
				/* check if this is an enum to be added */
				if (!std::holds_alternative<arger::Enum>(type))
					return;

				/* add the separate keys */
				for (const auto& val : std::get<arger::Enum>(type)) {
					fAddNewLine(false);
					fAddString(str::wd::Build(L"- [", val.name, L"]: ", val.description), detail::NumCharsHelpLeft);
				}
			}
			void fDefaultDescription(const arger::Value* begin, const arger::Value* end) {
				/* construct the list of all default values */
				std::wstring temp = L"Defaults to: [";
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
				temp.append(1, L']');

				/* write the line out */
				fAddNewLine(false);
				fAddString(temp, detail::NumCharsHelpLeft);
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
			constexpr void fHelpContent(const detail::Help& help) {
				/* iterate over the help-content and append it to the help-buffer */
				for (size_t i = 0; i < help.help.size(); ++i) {
					fAddNewLine(true);
					fAddString(help.help[i].name + L": ");
					fAddString(help.help[i].text, detail::NumCharsHelpLeft);
				}
			}

		private:
			constexpr void fRecGroupUsage(const detail::ValidArguments* topMost) {
				if (topMost == 0 || topMost->super == 0)
					return;
				fRecGroupUsage(topMost->super);
				fAddSpacedToken(topMost->group->name);
			}
			constexpr void fRecGroupDescription(const detail::ValidArguments* topMost) {
				/* add the description of all parents */
				if (topMost == 0 || topMost->super == 0)
					return;
				fRecGroupDescription(topMost->super);
				if (topMost->group->description.empty())
					return;

				/* add the newline, description header and description itself */
				fAddNewLine(true);
				fAddString(str::wd::Build(str::View{ topMost->super->groupName }.title(), L": ", topMost->group->name));
				fAddString(topMost->group->description, 4);
			}
			constexpr void fRecHelpStrings(const detail::ValidArguments* topMost) {
				if (topMost == 0 || topMost->super == 0)
					return;
				fRecHelpStrings(topMost->super);
				fHelpContent(*topMost->group);
			}
			void fAddEndpointDescription(const detail::ValidEndpoint& endpoint) {
				/* add the positional parameter for the topmost argument-object */
				for (size_t i = 0; i < endpoint.positionals->size(); ++i) {
					std::wstring token = (*endpoint.positionals)[i].name;

					if (i + 1 >= endpoint.positionals->size() && (endpoint.maximum == 0 || i + 1 < endpoint.maximum))
						token += L"...";
					if (i >= endpoint.minimum)
						token = L"[" + token + L']';
					fAddSpacedToken(token);
				}
			}

		private:
			bool fCheckOptionPrint(const detail::ValidOption* option, bool menu) const {
				/* in normal mode, check if the argument is excluded by the current selected group, based on the usage-requirements */
				if (!menu)
					return detail::CheckUsage(option, pTopMost);

				/* in menu-mode, print only flags, which are only available because a parent uses it,
				*	and for the special case of the root, only print it if no other groups exist */
				if (pTopMost->super == 0)
					return pConfig.sub.empty();
				for (const auto& user : option->users) {
					if (detail::CheckParent(user, pTopMost))
						return true;
				}
				return false;
			}
			void fBuildUsage(bool menu) {
				/* add the example-usage descriptive line */
				fAddToken(menu ? L"Input>" : L"Usage: ");
				if (!menu)
					fAddToken(pBase.program());

				/* add the already selected groups and potentially upcoming group-name */
				fRecGroupUsage(pTopMost);
				if (pTopMost->endpoints.empty())
					fAddSpacedToken(str::wd::Build(L'[', pTopMost->groupName, L']'));

				/* add all required options (must all consume a payload, as flags are never required) */
				bool hasOptionals = false;
				for (const auto& [name, option] : pConfig.options) {
					/* check if options exist in theory */
					if (detail::CheckUsage(&option, pTopMost) && option.minimum == 0) {
						hasOptionals = true;
						continue;
					}

					/* check if the entry can be skipped for this group */
					if (!fCheckOptionPrint(&option, menu))
						continue;
					fAddSpacedToken(str::wd::Build(L"--", name, L"=<", option.option->payload.name, L">"));
				}
				if (hasOptionals)
					fAddSpacedToken(L"[options...]");

				/* add the hint to the arguments after the final group has been selected */
				if (pTopMost->endpoints.empty() && pTopMost->nestedPositionals)
					fAddSpacedToken(L"[params...]");

				/* add all of the separate endpoints */
				for (size_t i = 0; i < pTopMost->endpoints.size(); ++i) {
					if (pTopMost->endpoints.size() > 1) {
						fAddNewLine(false);
						fAddString(str::wd::Build(L"  [Variation ", i + 1, L"]:"));
					}
					fAddEndpointDescription(pTopMost->endpoints[i]);
				}
			}
			void fBuildOptions(bool required, bool menu) {
				/* collect all of the names to be used */
				std::map<std::wstring, NameCache> selected;
				if (!menu && !required && pConfig.help != 0)
					selected.insert({ pConfig.help->name, NameCache{ &pConfig.help->description, 0, pConfig.help->abbreviation } });
				if (!menu && !required && pConfig.version != 0)
					selected.insert({ pConfig.version->name, NameCache{ &pConfig.version->description, 0, pConfig.version->abbreviation } });
				for (const auto& [name, option] : pConfig.options) {
					if ((option.minimum > 0) != required)
						continue;

					/* check if the option can be discarded based on the selection */
					if (!fCheckOptionPrint(&option, menu))
						continue;
					selected.insert({ name, NameCache{ &option.option->description, &option, option.option->abbreviation } });
				}

				/* iterate over the optionals and print them */
				for (const auto& [name, cache] : selected) {
					fAddNewLine(false);

					/* add the abbreviation and name */
					std::wstring temp = L"  ";
					if (cache.abbreviation != 0)
						temp.append(1, L'-').append(1, cache.abbreviation).append(L", ");
					temp.append(L"--").append(name);
					const detail::ValidOption* option = cache.option;

					/* add the payload */
					if (option != 0 && option->payload)
						temp.append(L"=<").append(option->option->payload.name).append(1, L'>').append(fTypeString(option->option->payload.type));
					fAddString(temp);

					/* add the description text and custom usage-limits and write the result out */
					temp = *cache.description;
					if (option != 0 && (option->minimum != 1 || option->maximum != 1))
						temp.append(fLimitString(option->minimum, option->maximum > 1 ? option->maximum : 0));
					fAddString(temp, detail::NumCharsHelpLeft, 1);

					/* check if this was a special entry, in which case no additional data will be added */
					if (option == 0)
						continue;

					/* add the enum value description */
					fEnumDescription(option->option->payload.type);

					/* add the default values */
					if (!option->option->payload.defValue.empty())
						fDefaultDescription(&option->option->payload.defValue.front(), &option->option->payload.defValue.back() + 1);

					/* add the usages (if groups still need to be selected and not all are users) */
					if (!pTopMost->endpoints.empty())
						continue;
					size_t users = 0;
					for (const auto& [name, group] : pTopMost->sub) {
						if (detail::CheckUsage(option, &group))
							++users;
					}
					if (users != pTopMost->sub.size()) {
						fAddNewLine(false);
						temp = L"Used for: ";
						size_t index = 0;
						for (const auto& group : pTopMost->sub) {
							if (option->users.contains(&group.second))
								temp.append(index++ > 0 ? L"|" : L"").append(group.first);
						}
						fAddString(temp, detail::NumCharsHelpLeft);
					}
				}
			}

		public:
			std::wstring buildHelpString(bool menu) {
				/* add the example-usage descriptive line */
				fBuildUsage(menu);

				/* add the program description */
				if (!pConfig.config->description.empty()) {
					fAddNewLine(true);
					fAddString(pConfig.config->description, 4);
				}

				/* add the group description */
				fRecGroupDescription(pTopMost);

				/* add the description of all sub-groups */
				if (pTopMost->endpoints.empty() || (menu && (pConfig.help != 0 || pConfig.version != 0))) {
					fAddNewLine(true);
					if (pTopMost->endpoints.empty())
						fAddString(str::wd::Build(L"Options for [", pTopMost->groupName, L"]:"));
					else
						fAddString(str::wd::Build(L"Optional Keywords:"));

					/* collect the list of all names to be used */
					std::map<std::wstring, NameCache> selected;
					if (menu && pConfig.help != 0)
						selected.insert({ pConfig.help->name, NameCache{ &pConfig.help->description, 0, pConfig.help->abbreviation } });
					if (menu && pConfig.version != 0)
						selected.insert({ pConfig.version->name, NameCache{ &pConfig.version->description, 0, pConfig.version->abbreviation } });
					for (const auto& [name, group] : pTopMost->sub)
						selected.insert({ name, NameCache{ &group.group->description, 0, group.group->abbreviation } });

					/* print all of the selected entries */
					for (const auto& [name, cache] : selected) {
						fAddNewLine(false);
						fAddString(str::wd::Build(L"  ", name));
						if (cache.abbreviation != 0)
							fAddString(str::wd::Build(L", ", cache.abbreviation));
						if (!cache.description->empty())
							fAddString(*cache.description, detail::NumCharsHelpLeft, 1);
					}
				}

				/* add the endpoints */
				if (!pTopMost->endpoints.empty()) {
					/* add the separate endpoints */
					for (size_t i = 0; i < pTopMost->endpoints.size(); ++i) {
						fAddNewLine(true);
						if (pTopMost->endpoints.size() == 1)
							fAddString(L"Positional Arguments:");
						else {
							fAddString(str::wd::Build(L"Positional Arguments [Variation ", i + 1, L"]:"));
							fAddEndpointDescription(pTopMost->endpoints[i]);
						}
						const detail::ValidEndpoint& endpoint = pTopMost->endpoints[i];

						/* add the positional arguments descriptions (will automatically be sorted by position) */
						for (size_t j = 0; j < endpoint.positionals->size(); ++j) {
							const detail::Positionals::Entry& positional = (*endpoint.positionals)[j];
							fAddNewLine(false);

							/* add the name and corresponding type and description */
							fAddString(str::wd::Build("  ", positional.name, fTypeString(positional.type), L" "));
							std::wstring temp = positional.description;
							if (j + 1 >= endpoint.positionals->size() && j + 1 < endpoint.maximum)
								temp.append(fLimitString(std::max<intptr_t>(endpoint.minimum - intptr_t(j), 0), endpoint.maximum - j));
							fAddString(temp, detail::NumCharsHelpLeft, 1);

							/* add the enum value description */
							fEnumDescription(positional.type);

							/* add the default values */
							if (positional.defValue.has_value())
								fDefaultDescription(&positional.defValue.value(), &positional.defValue.value() + 1);
						}

						/* add the description of the endpoint */
						if (endpoint.description != 0) {
							fAddNewLine(true);
							fAddString(*endpoint.description, 4);
						}
					}
				}

				/* check if there are optional/required arguments */
				bool optArgs = false, reqArgs = false;
				for (const auto& entry : pConfig.options) {
					if (fCheckOptionPrint(&entry.second, menu))
						(entry.second.minimum > 0 ? reqArgs : optArgs) = true;
				}

				/* add the required argument descriptions (will automatically be sorted lexicographically) */
				if (reqArgs) {
					fAddNewLine(true);
					fAddString(L"Required arguments:");
					fBuildOptions(true, menu);
				}

				/* add the optional argument descriptions (will automatically be sorted lexicographically) */
				if (optArgs) {
					fAddNewLine(true);
					fAddString(L"Optional arguments:");
					fBuildOptions(false, menu);
				}

				/* add all of the global help descriptions and the help-content of the selected groups */
				fHelpContent(*pConfig.config);
				fRecHelpStrings(pTopMost);

				/* return the constructed help-string */
				std::wstring out;
				std::swap(out, pBuffer);
				return out;
			}
		};
	}

	/* construct help-hint suggesting to use '--help' */
	inline constexpr std::wstring HelpHint(const std::vector<std::wstring>& args, const arger::Config& config) {
		return detail::BaseBuilder{ (args.empty() ? L"" : args[0]), config, false }.buildHelpHintString();
	}
}
