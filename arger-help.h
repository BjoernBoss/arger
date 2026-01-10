/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2024-2026 Bjoern Boss Henrichsen */
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
	static constexpr std::string_view VisualPadding = "   ";

	namespace detail {
		class BaseBuilder {
		private:
			const detail::Config& pConfig;
			std::string pProgram;

		public:
			BaseBuilder(const std::string& firstArg, const arger::Config& config) : pConfig{ detail::ConfigBurner::GetBurned(config) } {
				/* check if the first argument contains a valid program-name */
				std::string_view view{ firstArg };
				size_t begin = view.size();
				while (begin > 0 && view[begin - 1] != '/' && view[begin - 1] != '\\')
					--begin;

				/* setup the final program name (if the first argument is an empty string, the configured program name will be used) */
				pProgram = (begin == view.size() ? pConfig.program : std::string{ view.substr(begin) });
			}

		public:
			constexpr std::string buildVersionString() const {
				/* version string is guaranteed to never be empty if it can be requested to be built */
				if (pConfig.program.empty())
					return pConfig.version;
				if (pProgram.empty())
					throw arger::ConfigException{ "Configuration must have a program name." };
				return str::ch::Build(pProgram, ' ', pConfig.version);
			}
			constexpr std::string buildHelpHintString() const {
				if (pConfig.special.help.name.empty())
					throw arger::ConfigException{ "Help entry name must not be empty for help-hint string." };
				if (pConfig.program.empty())
					return str::ch::Build("Use '", pConfig.special.help.name, "' for more information.");
				if (pProgram.empty())
					throw arger::ConfigException{ "Configuration must have a program name." };
				return str::ch::Build("Try '", pProgram, " --", pConfig.special.help.name, "' for more information.");
			}
			constexpr const std::string& program() const {
				return pProgram;
			}
		};

		class HelpBuilder {
		private:
			struct NameCache {
				std::string used;
				const detail::Description* description = 0;
				const detail::ValidOption* option = 0;
				char abbreviation = 0;
			};

		private:
			std::string pBuffer;
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
				if (pBuffer.back() != '\n')
					pBuffer.push_back('\n');
				if (emptyLine)
					pBuffer.push_back('\n');
				pPosition = 0;
				pOpenWhiteSpace = 0;
			}
			constexpr void fAddToken(const std::string& add) {
				if (pPosition > 0 && pPosition + add.size() > pNumChars) {
					pBuffer.push_back('\n');
					pPosition = 0;
				}

				pBuffer.append(add);
				pPosition += add.size();
				pOpenWhiteSpace = 0;
			}
			constexpr void fAddSpacedToken(const std::string& add) {
				if (pPosition > 0) {
					if (pPosition + 1 + add.size() > pNumChars) {
						pBuffer.push_back('\n');
						pPosition = 0;
					}
					else {
						pBuffer.push_back(' ');
						++pPosition;
					}
				}
				fAddToken(add);
			}
			constexpr void fAddString(std::string_view add, size_t offset = 0, size_t indentAutoBreaks = 0) {
				std::string tokenPrint;
				bool isWhitespace = true;

				/* ensure the initial indentation is valid */
				if (offset > 0) {
					if (pPosition + pOpenWhiteSpace >= offset) {
						pBuffer.push_back('\n');
						pPosition = 0;
					}
					pBuffer.append(offset - pPosition, ' ');
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
							pBuffer.append(1, '\n').append(pPosition = offset, ' ');
						else {
							pBuffer.append(pOpenWhiteSpace, ' ');
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
					if (add[i] == '\n')
						pBuffer.append(1, '\n').append(pPosition = offset, ' ');
					else if (add[i] == '\t')
						pOpenWhiteSpace += 4;
					else
						++pOpenWhiteSpace;
				}
			}

		private:
			constexpr std::string fLimitDescription(std::optional<size_t> minimum, std::optional<size_t> maximum) {
				if (minimum.has_value()) {
					/* check if both a minimum and maximum exist */
					if (maximum.has_value()) {
						if (*minimum == *maximum)
							return str::ch::Build(*minimum, 'x');
						return str::ch::Build(*minimum, "...", *maximum);
					}

					/* only a minimum exists */
					return str::ch::Build(*minimum, "...");
				}

				/* only a maximum exists */
				else if (maximum.has_value())
					return str::ch::Build("...", *maximum);
				return "";
			}
			std::string fDefaultDescription(const arger::Value* begin, const arger::Value* end, bool isEnumType) {
				/* construct the list of all default values */
				std::string temp = "Default: ";
				for (const arger::Value* it = begin; it != end; ++it) {
					temp.append(it == begin ? "" : ", ");

					if (it->isStr()) {
						if (isEnumType)
							temp.append(it->str());
						else
							temp.append(1, '\"').append(it->str()).append(1, '\"');
					}
					else if (it->isUNum())
						str::IntTo(temp, it->unum());
					else if (it->isINum())
						str::IntTo(temp, it->inum());
					else if (it->isReal())
						str::FloatTo(temp, it->real());
					else
						temp.append(it->boolean() ? "true" : "false");
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
					const std::string& text = ((pReduced && !val.reduced.empty()) ? val.reduced : val.normal);
					fAddString(str::ch::Format(" - [{:<{}}]: {}", val.name, length, text), arger::NumCharsHelpLeft, 7 + length);
				}
			}
			constexpr const char* fTypeString(const arger::Type& type) {
				if (std::holds_alternative<arger::Enum>(type))
					return " [enum]";
				arger::Primitive actual = std::get<arger::Primitive>(type);
				if (actual == arger::Primitive::boolean)
					return " [bool]";
				if (actual == arger::Primitive::unum)
					return " [uint]";
				if (actual == arger::Primitive::inum)
					return " [int]";
				if (actual == arger::Primitive::real)
					return " [real]";
				return "";
			}
			std::string fEndpointName(const detail::ValidEndpoint& endpoint, size_t index) {
				std::string token = (*endpoint.positionals)[index].name;
				if (index + 1 >= endpoint.positionals->size() && (endpoint.maximum == 0 || index + 1 < endpoint.maximum))
					token += "...";
				if (index >= endpoint.minimumEffective)
					token = "[" + token + ']';
				return token;
			}
			const std::string* fGetDescription(const detail::Description* desc) {
				if (desc == nullptr)
					return nullptr;
				const std::string& text = ((!pReduced || desc->description.reduced.empty()) ? desc->description.normal : desc->description.reduced);
				return (text.empty() ? nullptr : &text);
			}

		private:
			constexpr void fRecGroupUsage(const detail::ValidArguments* topMost) {
				if (topMost == nullptr || topMost->super == nullptr)
					return;
				fRecGroupUsage(topMost->super);
				fAddSpacedToken(topMost->group->name);
			}
			constexpr std::string fGroupNameHistory(const detail::ValidArguments* topMost) {
				if (topMost->super == nullptr)
					return "";
				std::string parents = fGroupNameHistory(topMost->super);
				return str::ch::Build(parents, (parents.empty() ? "[" : " > ["), str::View{ topMost->super->groupName }.title(), ": ", topMost->group->name, ']');
			}
			void fAddEndpointUsage(const detail::ValidEndpoint& endpoint) {
				/* add the positional parameter for the topmost argument-object */
				for (size_t i = 0; i < endpoint.positionals->size(); ++i)
					fAddSpacedToken(fEndpointName(endpoint, i));
			}
			bool fSelectSpecial(std::map<std::string, NameCache>& selected) {
				bool added = false;
				if (pConfig.help != nullptr && (pTopMost->super == nullptr || pConfig.help->allChildren.value_or(true))) {
					selected.insert({ pConfig.help->name, NameCache{ "", pConfig.help, nullptr, pConfig.help->abbreviation } });
					added = true;
				}
				if (pConfig.version != nullptr && (pTopMost->super == nullptr || pConfig.version->allChildren.value_or(true))) {
					selected.insert({ pConfig.version->name, NameCache{ "", pConfig.version, nullptr, pConfig.version->abbreviation } });
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
				for (const auto& link : option->links) {
					if (detail::CheckParent(link, pTopMost))
						return true;
				}
				return false;
			}
			void fBuildUsage() {
				/* add the example-usage descriptive line */
				fAddToken(pConfig.burned->program.empty() ? "Input>" : "Usage: ");
				if (!pConfig.burned->program.empty())
					fAddToken(pBase.program());

				/* add the already selected groups and potentially upcoming group-name */
				fRecGroupUsage(pTopMost);
				if (pTopMost->endpoints.empty())
					fAddSpacedToken(str::ch::Build('[', pTopMost->groupName, ']'));

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

					/* check if the entry should be added for the current group */
					if (fCheckOptionPrint(&option))
						fAddSpacedToken(str::ch::Build("--", name, "=<", option.option->payload.name, ">"));
				}
				if (hasOptionals)
					fAddSpacedToken("[options...]");

				/* add the hint to the arguments after the final group has been selected and add the endpoint hint */
				if (pTopMost->endpoints.empty() && pTopMost->nestedPositionals)
					fAddSpacedToken("[args...]");
				else if (pTopMost->endpoints.size() > 1)
					fAddString(" variation...");

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
							fAddString(str::ch::Build("  [Variation ", ++index, "]:"));
						}
						fAddEndpointUsage(pTopMost->endpoints[i]);
					}
				}
			}
			void fBuildOptions(bool required) {
				std::map<std::string, NameCache> selected;
				std::set<std::string> usedList;

				/* collect all of the names to be used (add the help/version options for programs and select
				*	the used-by lists, which will automatically be lexicographically sorted in itself) */
				if (!pConfig.burned->program.empty() && !required && fSelectSpecial(selected))
					usedList.insert("");
				for (const auto& [name, option] : pConfig.options) {
					if (option.hidden)
						continue;

					/* check if the option can be discarded based on the selection */
					if ((option.minimumEffective > 0) != required || !fCheckOptionPrint(&option))
						continue;
					std::string used;

					/* check if the option is optional and should not be printed for children */
					if (!required && !option.option->allChildren.value_or(true) && option.owner != pTopMost && pReduced)
						continue;

					/* collect the usages (if groups still need to be selected and not all visible groups are users) */
					if (pTopMost->endpoints.empty() && (pReduced ? pTopMost->reducedGroupOptions : pTopMost->normalGroupOptions)) {
						bool partial = false;
						for (const auto& [_, group] : pTopMost->sub) {
							if (!group.hidden && !detail::CheckUsage(&option, &group)) {
								partial = true;
								break;
							}
						}
						if (partial) for (const auto& [name, group] : pTopMost->sub) {
							if (!group.hidden && detail::CheckUsage(&option, &group))
								used.append(used.empty() ? "" : ", ").append(name);
						}
					}
					selected.insert({ name, NameCache{ used, option.option, &option, option.option->abbreviation } });
					usedList.insert(used);
				}

				/* iterate over the used-lists and optionals and print them (will automatically be
				*	lexicographically sorted by used-list, with empty first, and then by option name) */
				for (const std::string& used : usedList) {
					/* print the used-header */
					fAddNewLine(true);
					std::string temp = str::ch::Build(required ? "Required" : "Optional");
					if (used.empty())
						temp.push_back(':');
					else
						str::BuildTo(temp, " for [", used, "]: ");
					fAddString(temp);

					/* print the optionals, which are part of the used list */
					for (const auto& [name, cache] : selected) {
						if (cache.used != used)
							continue;
						fAddNewLine(false);

						/* add the abbreviation and name */
						std::string temp = "  ";
						if (cache.abbreviation != 0)
							temp.append(1, '-').append(1, cache.abbreviation).append(", ");
						temp.append("--").append(name);
						const detail::ValidOption* option = cache.option;

						/* add the payload */
						if (option != nullptr && option->payload)
							temp.append("=<").append(option->option->payload.name).append(fTypeString(option->option->payload.type)).append(1, '>');
						temp.append(VisualPadding);
						fAddString(temp);

						/* add the custom usage-limits and default values */
						temp.clear();
						if (option != nullptr) {
							std::string limit, defDesc;

							/* add the usage limits */
							if (option->minimumActual != 0 || option->maximum != 1) {
								if (option->minimumActual == 0)
									limit = (option->maximum > 0 ? fLimitDescription(std::nullopt, option->maximum) : fLimitDescription(0, std::nullopt));
								else
									limit = fLimitDescription(option->minimumActual, (option->maximum == 0 ? std::nullopt : std::optional<size_t>{ option->maximum }));
							}

							/* add the default values and write the additional information out */
							if (const std::vector<arger::Value>& defValue = option->option->payload.defValue; !defValue.empty())
								defDesc = fDefaultDescription(&defValue.front(), &defValue.back() + 1, std::holds_alternative<arger::Enum>(option->option->payload.type));
							if (!limit.empty() || !defDesc.empty())
								temp = str::ch::Build('[', limit, ((limit.empty() || defDesc.empty()) ? "" : "; "), defDesc, ']');
						}

						/* write the description out and check if the enum descriptions need to be written out as well */
						if (const std::string* text = fGetDescription(cache.description); text != nullptr)
							temp.append((temp.empty() ? 0 : 1), ' ').append(*text);
						fAddString(temp, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
						if (option != nullptr)
							fAddEnumDescription(option->option->payload.type);
					}
				}
			}
			void fBuildGroups() {
				/* collect the list of all names to be used (add the help and version entries for menus) */
				std::map<std::string, NameCache> selected;
				if (pConfig.burned->program.empty())
					fSelectSpecial(selected);
				if (pTopMost->endpoints.empty()) for (const auto& [name, group] : pTopMost->sub) {
					if (!group.hidden)
						selected.insert({ name, NameCache{ "", group.group, nullptr, group.group->abbreviation } });
				}

				/* check if anything actually needs to be printed */
				if (selected.empty())
					return;

				/* print the header */
				fAddNewLine(true);
				if (pTopMost->endpoints.empty())
					fAddString(str::ch::Build("Defined for [", pTopMost->groupName, "]:"));
				else
					fAddString("Additional:");

				/* print all of the selected entries */
				for (const auto& [name, cache] : selected) {
					fAddNewLine(false);
					fAddString(str::ch::Build("  ", name));
					if (cache.abbreviation != 0)
						fAddString(str::ch::Build(", ", cache.abbreviation));
					if (const std::string* text = fGetDescription(cache.description); text != nullptr)
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
						fAddString(pConfig.burned->program.empty() ? "Arguments:" : "Positional Arguments:");
					else {
						fAddString(str::ch::Build("Variation ", ++index, ':'));
						fAddEndpointUsage(pTopMost->endpoints[i]);
						fAddString(VisualPadding);
						if (const std::string* text = fGetDescription(endpoint.description); text != nullptr)
							fAddString(*text, arger::NumCharsHelpLeft, arger::AutoIndentLongText);
					}

					/* add the positional arguments descriptions (will automatically be sorted by position) */
					for (size_t j = 0; j < endpoint.positionals->size(); ++j) {
						const detail::Positional& positional = (*endpoint.positionals)[j];
						fAddNewLine(false);

						/* add the name and corresponding type and description */
						fAddString(str::ch::Build("  ", fEndpointName(endpoint, j), fTypeString(positional.type), VisualPadding));
						std::string temp, limit, defDesc;

						/* construct the limit string (only for the last entry) */
						if (j + 1 >= endpoint.positionals->size() && (endpoint.minimumEffective > j + 1 || endpoint.maximum > j + 1)) {
							if (endpoint.minimumEffective > j)
								limit = fLimitDescription(endpoint.minimumEffective - j, (endpoint.maximum > j ? std::optional<size_t>{ endpoint.maximum - j } : std::nullopt));
							else if (endpoint.maximum > j)
								limit = fLimitDescription(std::nullopt, endpoint.maximum - j);
						}

						/* add the default value and write the data out */
						if (positional.defValue.has_value())
							defDesc = fDefaultDescription(&*positional.defValue, &*positional.defValue + 1, std::holds_alternative<arger::Enum>(positional.type));
						if (!limit.empty() || !defDesc.empty())
							temp = str::ch::Build('[', limit, ((limit.empty() || defDesc.empty()) ? "" : "; "), defDesc, ']');

						/* add the description and append the enum value description */
						if (const std::string* text = fGetDescription(&positional); text != nullptr)
							temp.append((temp.empty() ? 0 : 1), ' ').append(*text);
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
				const std::string* desc = fGetDescription(topMost->group);
				if (desc == nullptr)
					return;

				/* add the newline, description header, and description itself */
				fAddNewLine(true);
				if (!pReduced)
					fAddString(fGroupNameHistory(topMost));
				fAddString(*desc, arger::IndentInformation);
			}
			void fBuildInformation() {
				/* iterate over the information-content and append it to the buffer */
				for (const auto& info : pConfig.infos) {
					if (pReduced && info.info->reduced.empty())
						continue;

					/* check if the information is not directly linked, in which
					*	case its parent must be directly linked, for it to be shown */
					if (!info.links.contains(pTopMost)) {
						if (!info.info->allChildren.value_or(false))
							continue;
						const detail::ValidArguments* walker = pTopMost->super;
						while (walker != nullptr && !info.links.contains(walker))
							walker = walker->super;
						if (walker == nullptr)
							continue;
					}

					/* write the information out */
					fAddNewLine(true);
					fAddString(info.info->name + ": ");
					fAddString((pReduced ? info.info->reduced : info.info->text), arger::NumCharsHelpLeft);
				}
			}

		public:
			std::string buildHelpString() {
				/* update top-most to the first not-hidden entry (root config can never be hidden) */
				while (pTopMost->super != nullptr && pTopMost->hidden)
					pTopMost = pTopMost->super;

				/* add the example-usage descriptive line */
				fBuildUsage();

				/* add the program description */
				if (const std::string* text = fGetDescription(pConfig.burned); text != nullptr && (!pReduced || pTopMost->super == nullptr)) {
					fAddNewLine(true);
					fAddString(*text, arger::IndentInformation);
				}

				/* build all of the components in order to the output */
				fBuildGroupDescription(pTopMost);
				fBuildGroups();
				fBuildEndpoints();
				fBuildOptions(true);
				fBuildOptions(false);
				fBuildInformation();

				/* return the constructed help-string */
				std::string out;
				std::swap(out, pBuffer);
				return out;
			}
		};
	}

	/* construct help-hint suggesting to use the help entry (for example 'try --help', or 'use help') */
	inline std::string HelpHint(const std::vector<std::string>& args, const arger::Config& config) {
		return detail::BaseBuilder{ (args.empty() ? "" : args[0]), config }.buildHelpHintString();
	}
}
