/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024 Bjoern Boss Henrichsen */
#pragma once

#include "arger-parsed.h"
#include "arger-config.h"
#include "arger-verify.h"

namespace arger {
	namespace detail {
		static constexpr size_t NumCharsHelpLeft = 32;
		static constexpr size_t NumCharsHelp = 100;

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
			std::wstring pBuffer;
			size_t pPosition = 0;
			const detail::ValidGroup* pSelected = 0;
			const detail::ValidConfig& pConfig;
			const detail::BaseBuilder& pBase;

		public:
			constexpr HelpBuilder(const detail::BaseBuilder& base, const detail::ValidConfig& config, const detail::ValidGroup* selected) : pBase{ base }, pConfig{ config }, pSelected{ selected } {}

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
				if (pPosition > 0 && pPosition + add.size() > detail::NumCharsHelp) {
					pBuffer.push_back(L'\n');
					pPosition = 0;
				}

				pBuffer.append(add);
				pPosition += add.size();
			}
			constexpr void fAddSpacedToken(const std::wstring& add) {
				if (pPosition > 0) {
					if (pPosition + 1 + add.size() > detail::NumCharsHelp) {
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
						if ((pPosition > offset || tokenWhite.size() > 0) && pPosition + tokenWhite.size() + tokenPrint.size() > detail::NumCharsHelp)
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
					fAddString(str::wd::Build(L"- [", val.first, L"]: ", val.second), detail::NumCharsHelpLeft);
				}
			}
			void fDefaultDescription(const arger::Value* begin, const arger::Value* end) {
				/* construct the list of all default values */
				std::wstring temp = L"Defaults to: (";
				for (const arger::Value* it = begin; it != end; ++it) {
					temp.append(it == begin ? L"[" : L", [");

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
					temp.append(1, L']');
				}
				temp.append(1, L')');

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
					fAddString(help.help[i].name + L" ");
					fAddString(help.help[i].text, detail::NumCharsHelpLeft);
				}
			}

		private:
			constexpr void fRecGroupUsage(const detail::ValidGroup* selected, const detail::ValidArguments* self) {
				if (selected == 0)
					return;
				fRecGroupUsage(selected->parent, selected->super);
				fAddSpacedToken(selected->group->name);
			}
			constexpr void fRecGroupDescription(const detail::ValidGroup* selected) {
				/* add the description of all parents */
				if (selected == 0)
					return;
				fRecGroupDescription(selected->parent);
				if (selected->group->description.empty())
					return;

				/* add the newline, description header and description itself */
				fAddNewLine(true);
				fAddString(str::wd::Build(str::View{ selected->super->groupName }.title(), L": ", selected->group->name));
				fAddString(selected->group->description, 4);
			}
			constexpr void fRecHelpStrings(const detail::ValidGroup* selected) {
				if (selected == 0)
					return;
				fRecHelpStrings(selected->parent);
				fHelpContent(*selected->group);
			}

		private:
			void fBuildUsage(bool menu) {
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);

				/* add the example-usage descriptive line */
				fAddToken(menu ? L"Input>" : L"Usage: ");
				if (!menu)
					fAddToken(pBase.program());

				/* add the already selected groups and potentially upcoming group-name */
				fRecGroupUsage(pSelected, topMost);
				if (topMost->incomplete)
					fAddSpacedToken(str::wd::Build(L'[', topMost->groupName, L']'));

				/* add all required options (must all consume a payload, as flags are never required) */
				bool hasOptionals = false;
				for (const auto& [name, option] : pConfig.options) {
					/* check if the entry can be skipped for this group */
					if (option.restricted && !option.users.contains(pSelected))
						continue;
					if (option.minimum == 0) {
						hasOptionals = true;
						continue;
					}
					fAddSpacedToken(str::wd::Build(L"--", name, L"=<", option.option->payload.name, L">"));
				}
				if (hasOptionals)
					fAddSpacedToken(L"[options...]");

				/* add the positional parameter for the topmost argument-object */
				if (!topMost->incomplete) {
					for (size_t i = 0; i < topMost->args->positionals.size(); ++i) {
						std::wstring token = topMost->args->positionals[i].name;

						if (i + 1 >= topMost->args->positionals.size() && (topMost->maximum == 0 || i + 1 < topMost->maximum))
							token += L"...";
						if (i >= topMost->minimum)
							token = L"[" + token + L']';
						fAddSpacedToken(token);
					}
				}

				/* add the hint to the arguments after the final group has been selected */
				else if (topMost->nestedPositionals)
					fAddSpacedToken(L"[params...]");
			}
			void fBuildOptionals(bool required) {
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);

				/* iterate over the optionals and add the corresponding type */
				for (const auto& [name, option] : pConfig.options) {
					if ((option.minimum > 0) != required)
						continue;

					/* check if the argument is excluded by the current selected group, based on the usage-requirements */
					if (option.restricted && !option.users.contains(pSelected))
						continue;
					fAddNewLine(false);

					/* construct the left help string */
					std::wstring temp = L"  ";
					if (option.option->abbreviation != 0)
						temp.append(1, L'-').append(1, option.option->abbreviation).append(L", ");
					temp.append(L"--").append(name);
					if (option.payload)
						temp.append(L"=<").append(option.option->payload.name).append(1, L'>').append(fTypeString(option.option->payload.type));
					fAddString(temp);

					/* construct the description text (add the users, if the optional argument is not a
					*	general purpose argument, and there are still groups available to be selected) */
					temp = option.option->description;
					if (option.restricted && option.users.contains(pSelected) && topMost->incomplete) {
						temp += L" (Used for: ";
						size_t index = 0;
						for (const auto& group : topMost->sub) {
							if (option.users.contains(&group.second))
								temp.append(index++ > 0 ? L"|" : L"").append(group.first);
						}
						temp.append(1, L')');
					}

					/* add the custom usage-limits and write the result out */
					if (option.minimum != 1 || option.maximum != 1)
						temp.append(fLimitString(option.minimum, option.maximum > 1 ? option.maximum : 0));
					fAddString(temp, detail::NumCharsHelpLeft, 1);

					/* add the enum value description */
					fEnumDescription(option.option->payload.type);

					/* add the default values */
					if (!option.option->payload.defValue.empty())
						fDefaultDescription(&option.option->payload.defValue.front(), &option.option->payload.defValue.back() + 1);
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
				fRecGroupDescription(pSelected);

				/* add the description of all sub-groups */
				const detail::ValidArguments* topMost = (pSelected == 0 ? static_cast<const detail::ValidArguments*>(&pConfig) : pSelected);
				if (topMost->incomplete) {
					fAddNewLine(true);
					fAddString(str::wd::Build(L"Options for [", topMost->groupName, L"]:"));
					for (const auto& [name, group] : topMost->sub) {
						fAddNewLine(false);
						fAddString(str::wd::Build(L"  ", name));
						fAddString(group.group->description, detail::NumCharsHelpLeft, 1);
					}
				}

				/* add the positional arguments */
				else if (!topMost->args->positionals.empty()) {
					fAddNewLine(true);
					if (pSelected == 0)
						fAddString(L"Positional Arguments: ");
					else
						fAddString(str::wd::Build(L"Positional Arguments for ", topMost->super->groupName, " [", pSelected->group->name, "]: "));

					/* add the positional arguments descriptions (will automatically be sorted by position) */
					for (size_t i = 0; i < topMost->args->positionals.size(); ++i) {
						const detail::Positionals::Entry& positional = topMost->args->positionals[i];
						fAddNewLine(false);

						/* add the name and corresponding type and description */
						fAddString(str::wd::Build("  ", positional.name, fTypeString(positional.type), L" "));
						std::wstring temp = positional.description;
						if (i + 1 >= topMost->args->positionals.size() && i + 1 < topMost->maximum)
							temp.append(fLimitString(std::max<intptr_t>(topMost->minimum - intptr_t(i), 0), topMost->maximum - i));
						fAddString(temp, detail::NumCharsHelpLeft, 1);

						/* add the enum value description */
						fEnumDescription(positional.type);

						/* add the default values */
						if (positional.defValue.has_value())
							fDefaultDescription(&positional.defValue.value(), &positional.defValue.value() + 1);
					}
				}

				/* check if there are optional/required arguments */
				bool optArgs = false, reqArgs = false;
				for (const auto& entry : pConfig.options) {
					if (!entry.second.restricted || entry.second.users.contains(pSelected))
						(entry.second.minimum > 0 ? reqArgs : optArgs) = true;
				}

				/* add the required argument descriptions (will automatically be sorted lexicographically) */
				if (reqArgs) {
					fAddNewLine(true);
					fAddString(L"Required arguments:");
					fBuildOptionals(true);
				}

				/* add the optional argument descriptions (will automatically be sorted lexicographically) */
				if (optArgs) {
					fAddNewLine(true);
					fAddString(L"Optional arguments:");
					fBuildOptionals(false);
				}

				/* add all of the global help descriptions and the help-content of the selected groups */
				fHelpContent(*pConfig.config);
				fRecHelpStrings(pSelected);

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
