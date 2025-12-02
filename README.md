# Argument Parsing for C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Header-only library written in `C++20` to add simple argument parsing to C++. This library adds support for optional flags/arguments, as well as sub-commands (called `group`), and positional arguments. Grouping hereby describes the practice of having sub-commands as the corresponding next primary argument. An example for this would be `git`, where the first argument is the actual sub-command to be executed. It is a large library, which can be used for a variety of different program or menu interfaces.

The simple idea is to define the argument-layout using `arger::Config`. The `arger::Parse(size_t argc, const argv, const arger::Config& config)` method will then produce a `arger::Parsed` structure, which contains the final results.

A configuration can be used for program arguments or as command-line style menus. This will not require, nor print any program information and redesign the help menu to fit a command-line style menu. It will also turn the help and version entries from options to group keys, which can be used at any time. Menu mode or normal program mode is decided based on the existence of a pre-defined program name.

## Using the Library
This library is a header only library. Simply clone the repository, ensure that `./repos` is on the path (or at least that `<ustring/ustring.h>` can be resolved), and include `<arger/arger.h>`.

	$ git clone https://github.com/BjoernBoss/arger.git --recursive

Note: for convenience, all numerical argument types, can be given using any `radix` (provided the corresponding prefix is used), and can be extended using `Si` prefix scalings. I.e. `0x50G` is valid, and will result in `80 * 10^9`. This also includes binary `Si` types, such as `ki` and `Mi` as `2^10` and `2^20` respectively. I.e. `0b10000000000mi` will be equal to `1.0`.

## Philosophy of the Library

The core idea is to define the entire menu into a `arger::Config` object, which can be constructed as a single static object, such as in the examples below, or programatically, by calling the `add` method on any of the configurable objects.

The library defines multiple separate concepts:

- `Configuration`: The overall configuration of the entire menu.
- `Group`: A sub-group of commands. Similar to the configuration itself, but adds another nested layer to the menu. Any groups, options, and other configurations apply to it and its children.
- `Endpoint`: A final endpoint for positional arguments. They can be explicitly provided, which are differentiated by number of positional arguments, or are implicitly defined for a `Configuration` or `Group`, if none are given. If defined implicitly, the positional arguments are just defined on the `Group` or `Configuration` itself.
- `Option`: An option can be supplied optionally to the program, via `--example` or `-e`, with support for all common other command line practices. If an `Option` requires no payload, it is considered a `Flag`, which can either be present, or absent.

For `Endpoints` and `Options`, default values can be supplied. If less values are supplied, the `arger::Parsed` structure will always be filled up with the remaining default values.

### Normal and Reduced mode
The library differentiates between `normal` and `reduced` menu mode. If a help entry is defined, it can optionally define, if reduced mode is supported. If so, the abbreviation (such as `-h`) will print the reduced menu, instead of the full help menu. 

Many strings, such as descriptions and others, can be defined to be different for reduced mode. If none is defined, the normal text is either used (for example for the description of `Options`), or the text is not printed at all (for example for additional `Information`).

The core idea being, that `-h` helps you get a quick overview of the available commands, while `--help` gives a detailed description of everything, and potential explanation of additional core features.

### Check and Verification

Before a configuration is parsed or printed, it will first be verified. This verification check is performed on the data structure, and throws `arger::ConfigExceptions`, should a mis-configuration be detected. An example would be, defining requiring limits on a `Flag`. As it does not have a payload, it does not make sense to require it.

This also guarantees, that the returned `arger::Parsed` structure adheres to the defined `Configuration`. If the configuration requires a given `Endpoint` to have at least four positional arguments, the parser will only return, if at least four were provided. The program can therefore safely just access them.

To further perform checking, the parser runs all defined `arger::Constraints`, which apply. I.e., if they are defined for the root, an `Option`, or a given `Endpoint`, they will be executed, if the given path was chosen or `Option` provided. These can check the `arger::Parsed` structure, to validate further inter-dependent requirements, which cannot be encoded in the configuration itself.

## Configuration Options

There exist a set of configuration options, which can be applied to any of the conditionally configurable objects, provided they are meant for it (checked by the `arger::IsConfig` concept).

The following configurations are defined:

```C++
/* general arger-configuration to be parsed */
arger::Config(const arger::IsConfig<detail::Config> auto&... configs);

/* general optional flag or option (id's must be unique and are used to distinguish the options/flags in the parsed result)
*	Note: if passed to a group, it is implicitly only bound to that group - but all names and abbreviations must be unique
*	Note: if payload is provided, is not considered a flag, but an option */
arger::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Option> auto&... configs);

/* endpoint for positional arguments for a configuration/group (to enable a group to have multiple variations of positional counts)
*	 Note: If Groups/Configs define positional arguments directly, an implicit endpoint is defined and no further endpoints can be added */
arger::Endpoint(const arger::IsConfig<detail::Endpoint> auto&... configs);
arger::Endpoint(arger::IsId auto id, const arger::IsConfig<detail::Endpoint> auto&... configs);

/* general sub-group of options for a configuration/group
*	 Note: Groups/Configs can only either have sub-groups or positional arguments */
arger::Group(std::wstring name, const arger::IsConfig<detail::Group> auto&... configs);
arger::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Group> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which
*	triggers the help-menu to be printed (prior to verifying the remainder of the argument structure)
*	Note: if reducible is enabled, the usage of the abbreviation will only print the reduced help menu */
arger::HelpEntry(std::wstring name, bool reducible, const arger::IsConfig<detail::SpecialEntry> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which
*	triggers the version-menu to be printed (prior to verifying the remainder of the argument structure)
*	Note: if all-children is enabled, the help entry will be printed as option in all sub-groups as well */
arger::VersionEntry(std::wstring name, const arger::IsConfig<detail::SpecialEntry> auto&... configs);

/* version text for the current configuration (preceeded by program name, if not in menu-mode) */
arger::VersionText(std::wstring text);

/* default alternative program name for the configuration (no program implies menu mode) */
arger::Program(std::wstring program);

/* description to the corresponding object
*	Note: if reduced text is used, it will be used for reduced help menus */
arger::Description(std::wstring desc);
arger::Description(std::wstring reduced, std::wstring desc);

/* add information-string to the corresponding config/group and show them for entry and optionally all children (depending on reach)
*	Note: will print the information in the normal mode, and optionally adjusted in the reduced view */
arger::Information(std::wstring name, std::wstring text, const arger::IsConfig<detail::Information> auto&... configs);
arger::Information(std::wstring name, std::wstring reduced, std::wstring text, const arger::IsConfig<detail::Information> auto&... configs);

/* add a constraint to be executed if the corresponding object is selected via the arguments */
arger::Constraint(arger::Checker constraint);

/* add a minimum/maximum requirement [maximum < minimum implies no maximum]
*	if only minimum is supplied, default maximum will be used
*	- [Option]: are only allowed for non-flags with a default of [min: 0, max: 1]
*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
*		if greater than number of positional arguments, last type is used as catch-all */
arger::Require(size_t min = 1);
arger::Require(size_t min, size_t max);
arger::Require::AtLeast(size_t min);
arger::Require arger::Any();

/* add an abbreviation character for an option, group, or help/version entry to allow it to be accessible as single letters or, for example, -x */
arger::Abbreviation(wchar_t c);

/* set the endpoint id of the implicitly defined endpoint
*	Note: cannot be used in conjunction with explicitly defined endpoints */
arger::EndpointId(arger::IsId auto id);

/* add a payload to an option with a given name and of a given type */
arger::Payload(std::wstring name, arger::Type type);

/* add usage-constraints to let the corresponding options only be used by groups, which add
*	them as usage (by default the config/group, in which the option is defined, can use it) */
arger::Use(arger::IsId auto... options);

/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
arger::GroupName(std::wstring name);

/* add an additional positional argument to the configuration/group using the given name and type (and optional immediate description)
*	Note: Must meet the requirement-counts
*	Note: Groups/Configs can can only have sub-groups or positional arguments */
arger::Positional(std::wstring name, arger::Type type, std::wstring desc);
arger::Positional(std::wstring name, arger::Type type, const arger::IsConfig<detail::Positional> auto&... configs);

/* add a default value to a positional or one or more default values to optionals - will be used when
*	no values are povided and to fill up remaining values, if less were provided and must meet requirement
*	counts for optionals/must be applied to all upcoming positionals for the requirement count */
arger::Default(arger::Value defValue);

/* set the visibility of options/groups/endpoints and their children in the help menu (does not affect parsing) */
arger::Visibility(bool visible);

/* configure if the visibility of the entry for children
*	Note: for help/version, defaults to true, otherwise will only be printed in help menu of the root
*	Note: for information, defaults to false and will only be printed for the level it was defined at */
arger::Reach(bool allChildren);
```

## Common Command Line Mode

This simple example shows the configuration for a simple command line mode, without using sub-commands.
<br>
The following example configuration:

```C++
enum class Mode : uint8_t { abc, def };
enum class Option : uint8_t { test, path, mode };
arger::Config config{
	arger::Program{ L"test.exe" },
	arger::VersionText{ L"Version [1.0.1]" },
	arger::Information{ L"Some Description", L"This is the indepth description." },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test", Option::test,
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::HelpEntry{ L"help", true,
		arger::Abbreviation{ L'h' },
		arger::Description{ L"Print this help menu." }
	},
	arger::VersionEntry{ L"version",
		arger::Abbreviation{ L'v' },
		arger::Description{ L"Print the program version." }
	},
	arger::Option{ L"path", Option::path,
		arger::Abbreviation{ L'p' },
		arger::Payload{ L"file-path", arger::Primitive::any },
		arger::Description{ L"This is some path option" },
		arger::Require{ 1, 0 }
	},
	arger::Option{ L"mode", Option::mode,
		arger::Payload{ L"test-mode",
			arger::Enum{
				arger::EnumEntry{ L"abc", Mode::abc, L"This is the description of option abc" },
				arger::EnumEntry{ L"def", Mode::def, L"This is the description of option def" }
			},
		},
		arger::Default{ L"def" },
		arger::Description{ L"Example of an enum" }
	},
	arger::Require{ 2, 0 },
	arger::Positional{ L"first", arger::Primitive::unum, L"First Argument" },
	arger::Positional{ L"second", arger::Primitive::boolean, L"Second Argument" },
	arger::Positional{ L"third", arger::Primitive::any, L"Third Argument" },
	arger::Constraint{ [](const arger::Parsed& p) {
		if (p.positional(0)->unum() >= 50)
			return L"Argument must be less than 50";
		return L"";
	}}
};

try {
	arger::Parsed parsed = arger::Parse(argc, argv, config);
}
catch (const arger::ParsingException& err) {
	str::PrintLn(err.what(), L"\n\n", arger::HelpHint(argc, argv, config));
}
catch (const arger::PrintMessage& msg) {
	str::PrintLn(msg.what());
}
```

Constructs the following large help-page:
```
$ ./bin.exe --help
Usage: bin.exe --path=<file-path> [options...] first second [third...]

    Some test program

Positional Arguments:
  first [uint]                  First Argument
  second [bool]                 Second Argument
  third                         Third Argument

Required:
  -p, --path=<file-path>        [1...] This is some path option

Optional:
  -h, --help                    Print this help menu.
  --mode=<test-mode [enum]>     [Default: def] Example of an enum
                                 - [abc]: This is the description of option abc
                                 - [def]: This is the description of option def
  -t, --test                    This is the description of the test flag.
  -v, --version                 Print the program version.

Some Description:               This is the indepth description.
```

An example for the version:
```
$ ./bin.exe -v
bin.exe Version [1.0.1]
```

And examples of potential errors:

```
$ ./bin.exe 51 false --path=/foo/bar
Argument must be less than 50

Try 'bin.exe --help' for more information.
```
```
$ ./bin.exe 12 true --mode=ghi -tp=/foo/bar -- test1 test2 --mode=50
Invalid enum for option [mode] encountered.

Try 'bin.exe --help' for more information.
```


## Sub-Command Line Mode

To setup the sub-command line mode, add all sub-commands as groups using `arger::Group`. The term used in the help-output for `groups` can be configured using `arger::GroupName`. The default is `mode`.
<br>
The following example configuration:

```C++
enum class Mode : uint8_t { abc, def };
enum class Group : uint8_t { get, set, read };
enum class Option : uint8_t { test, path, mode };
arger::Config config{
	arger::Program{ L"test.exe" },
	arger::VersionText{ L"Version [1.0.1]" },
	arger::Information{ L"Some Description", L"This is the indepth description." },
	arger::GroupName{ L"test-setting" },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test", Option::test,
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::HelpEntry{ L"help", true,
		arger::Abbreviation{ L'h' },
		arger::Description{ L"Print this help menu." }
	},
	arger::Option{ L"path", Option::path,
		arger::Abbreviation{ L'p' },
		arger::Payload{ L"file-path", arger::Primitive::any },
		arger::Description{ L"This is some path option" },
		arger::Require{ 1 }
	},
	arger::Option{ L"mode", Option::mode,
		arger::Payload{ L"test-mode",
			arger::Enum{
				arger::EnumEntry{ L"abc", Mode::abc, L"Short text abc", L"This is the description of option abc" },
				arger::EnumEntry{ L"def", Mode::def, L"Short text def", L"This is the description of option def" }
			},
		},
		arger::Description{ L"Example of an enum" },
		arger::Default{ L"def" }
	},
	arger::Group{ L"get", Group::get,
		arger::Description{ L"Get something in the test program. But this is just a demo description" },
		arger::Require{ 2, 4 },
		arger::Positional{ L"first", arger::Primitive::unum, L"First Argument" },
		arger::Positional{ L"second", arger::Primitive::boolean, L"Second Argument" },
		arger::Positional{ L"third", arger::Primitive::any, L"Third Argument" },
		arger::Constraint{ [](const arger::Parsed& p) {
			if (p.positional(0)->unum() >= 50)
				return L"Argument must be less than 50";
			return L"";
		}},
		arger::Use{ Option::path }
	},

	arger::Group{ L"set", Group::set,
		arger::Description{ L"Set something in the test program. But this is just a demo description" },
		arger::Require{ 1 },
		arger::Positional{ L"first", arger::Primitive::unum, L"First Argument" },
		arger::Use{ Option::path }
	},
	arger::Group{ L"read", Group::read,
		arger::Require{ 2 },
		arger::Positional{ L"argument", arger::Enum{
			arger::EnumEntry{ L"a", 0, L"This is [a] description" },
			arger::EnumEntry{ L"b", 1, L"This is [b] description" }
		}, L"First Argument" },
		arger::Information{ L"Read-Help", L"This is the short text", L"This is a help-description only shown for read."}
	}
};

try {
	arger::Parsed parsed = arger::Parse(argc, argv, config);
}
catch (const arger::ParsingException& err) {
	str::PrintLn(err.what(), L"\n\n", arger::HelpHint(argc, argv, config));
}
catch (const arger::PrintMessage& msg) {
	str::PrintLn(msg.what());
}
```

Constructs the following help-page:
```
$ ./bin.exe --help
Usage: bin.exe [test-setting] --path=<file-path> [options...] [params...]

    Some test program

Defined for [test-setting]:
  get                           Get something in the test program. But this is just a demo
                                  description
  read
  set                           Set something in the test program. But this is just a demo
                                  description

Required for [get, set]:
  -p, --path=<file-path>        [1...] This is some path option

Optional:
  -h, --help                    Print this help menu.
  --mode=<test-mode [enum]>     [Default: def] Example of an enum
                                 - [abc]: This is the description of option abc
                                 - [def]: This is the description of option def
  -t, --test                    This is the description of the test flag.

Some Description:               This is the indepth description.
```

Constructs the following small help-page for a sub-group:

```
$ ./bin.exe read -h
Usage: bin.exe read [options...] argument...

Positional Arguments:
  argument [enum]               [2x] First Argument
                                 - [a]: This is [a] description
                                 - [b]: This is [b] description

Optional:
  -h, --help                    Print this help menu.
  --mode=<test-mode [enum]>     [Default: def] Example of an enum
                                 - [abc]: Short text abc
                                 - [def]: Short text def
  -t, --test                    This is the description of the test flag.

Read-Help:                      This is the short text
```

And examples of potential errors:

```
$ ./bin.exe
Test-Setting missing.

Try 'bin.exe --help' for more information.
```
```
$ ./bin.exe set 50
Option [path] is missing.

Try 'bin.exe --help' for more information.
```
```
$ ./bin.exe read -p 50 a b
Option [path] not meant for test-setting [read].

Try 'bin.exe --help' for more information.
```
