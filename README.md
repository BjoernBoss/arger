# Argument Parsing for C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Small header-only library written in `C++20` to add simple argument parsing to C++. This library adds support for optional flags/arguments, as well as sub-commands (called `group`), and positional arguments. Grouping hereby describes the practice of having sub-commands as the corresponding next primary argument. An example for this would be `git`, where the first argument is the actual sub-command to be executed.

The simple idea is to define the argument-layout using `arger::Config`. The `arger::Parse(int argc, const argv, const arger::Config& config)` method will then produce a `arger::Parsed` structure, which contains the final results.

A configuration can be used for program arguments or as command-line style menus. This will not require, nor print any program information and redesign the help menu to fit a command-line style menu. It will also turn the help and version entries from options to group keys, which can be used at any time. Menu mode or normal program mode is decided based on the existance of a pre-defined program name.

## Using the library
This library is a header only library. Simply clone the repository, ensure that `./repos` is on the path (or at least that `<ustring/ustring.h>` can be resolved), and include `<arger/arger.h>`.

	$ git clone https://github.com/BjoernBoss/arger.git --recursive

Note: for convenience, all numerical argument types, can be given using any `radix` (provided the corresponding prefix is used), and can be extended using `Si` prefix scalings. I.e. `0x50G` is valid, and will result in `80 * 10^9`. This also includes binary `Si` types, such as `ki` and `Mi` as `2^10` and `2^20` respectively. I.e. `0b10000000000mi` will be equal to `1.0`.

## Configuration Options

There exist a set of configuration options, which can either be applied to optional arguments (`arger::Option`), to groups (`arger::Group`) or the general configuration (`arger::Config`).
The following configurations are defined:

```C++
/* general arger-configuration to be parsed */
arger::Config(const arger::IsConfig<detail::Config> auto&... configs);

/* general sub-group of options for a configuration/group (id can be enum or int)
*	 Note: Groups/Configs can can only have sub-groups or positional arguments */
arger::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Group> auto&... configs);

/* endpoint for positional arguments for a configuration/group (to enable a group to have multiple variations of positional counts)
*	 Note: If Groups/Configs define positional arguments directly, an implicit endpoint is defined and no further endpoints can be added */
arger::Endpoint(arger::IsId auto id, const arger::IsConfig<detail::Endpoint> auto&... configs);

/* general optional flag/payload (id can be enum or int)
*	Note: if passed to a group, it is implicitly only bound to that group - but all names and abbreviations must be unique */
arger::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<detail::Option> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which
*	triggers the help-menu to be printed (prior to verifying the remainder of the argument structure)
*	Note: if reducible is enabled, the usage of the abbreviation will not print any additional information
*	Note: if all-children is enabled, the help entry will be printed as option in all sub-groups as well */
arger::HelpEntry(std::wstring name, bool allChildren, bool reducible, const arger::IsConfig<detail::Option> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which
*	triggers the version-menu to be printed (prior to verifying the remainder of the argument structure)
*	Note: if all-children is enabled, the help entry will be printed as option in all sub-groups as well */
arger::VersionEntry(std::wstring name, bool allChildren, const arger::IsConfig<detail::Option> auto&... configs);

/* version text for the current configuration (preceeded by program name, if not in menu-mode) */
arger::VersionText(std::wstring text);

/* default alternative program name for the configuration (no program implies menu mode) */
arger::Program(std::wstring program);

/* description to the corresponding object
*	Note: if reduced text is used, it will be used for reduced help menus */
arger::Description(std::wstring desc);
arger::Description(std::wstring reduced, std::wstring desc);

/* add information-string to the corresponding config/group and show them for entry and all children
*	Note: will print the information optionally in the normal, reduced, or both menus */
arger::Information(std::wstring name, bool normal, bool reduced, std::wstring text);

/* add a constraint to be executed if the corresponding object is selected via the arguments */
arger::Constraint(arger::Checker constraint);

/* add a minimum/maximum requirement [maximum < minimum implies no maximum]
*	if only minimum is supplied, default maximum will be used
*	- [Option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
*		if greater than number of positional arguments, last type is used as catch-all */
arger::Require(size_t min, size_t max);

/* add an abbreviation character for an option, group, or help/version entry to allow it to be accessible as single letters or, for example, -x */
arger::Abbreviation(wchar_t c);

/* set the endpoint id of the implicitly defined endpoint
*	Note: cannot be used in conjunction with explicitly defined endpoints */
arger::EndpointId(arger::IsId auto id);

/* add a payload to an option with a given name and of a given type */
arger::Payload(std::wstring name, arger::Type type);

/* add usage-constraints to let the corresponding options only be used by groups,
*	which add them as usage (by default every group/argument can use all options) */
arger::Use(arger::IsId auto... options);

/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
arger::GroupName(std::wstring name);

/* add an additional positional argument to the configuration/group using the given name, type, description
*	Note: Must meet the requirement-counts
*	Note: Groups/Configs can can only have sub-groups or positional arguments */
arger::Positional(std::wstring name, arger::Type type, const arger::IsConfig<detail::Positional> auto&... configs);

/* add a default value to a positional or one or more default values to optionals - will be used when
*	no values are povided and to fill up remaining values, if less were provided and must meet requirement
*	counts for optionals/must be applied to all upcoming positionals for the requirement count */
arger::Default(arger::Value defValue);
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
	arger::VersionText{ L"[1.0.1]" },
	arger::Information{ L"Some Description", L"This is the indepth description." },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test", Option::test,
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::HelpEntry{ L"help",
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
				arger::EnumEntry{ L"abc", L"This is the description of option abc", Mode::abc },
				arger::EnumEntry{ L"def", L"This is the description of option def", Mode::def }
			},
			arger::Value{ L"def" }
		},
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

Constructs the following help-page:
```
$ ./bin.exe --help
Usage: bin.exe --path=<file-path> [options...] first second [third...]

    Some test program

Positional Arguments:
  first [uint]                  First Argument
  second [bool]                 Second Argument
  third                         Third Argument

Required arguments:
  -p, --path=<file-path>        This is some path option [>= 1]

Optional arguments:
  -h, --help                    Print this help menu.
  --mode=<test-mode> [enum]     Example of an enum
                                - [abc]: This is the description of option abc
                                - [def]: This is the description of option def
                                Defaults to: ([def])
  -t, --test                    This is the description of the test flag.
  -v, --version                 Print the program version.

Some Description                This is the indepth description.
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
Invalid enum for argument [mode] encountered.

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
	arger::Version{ L"[1.0.1]" },
	arger::Information{ L"Some Description", L"This is the indepth description." },
	arger::GroupName{ L"test-setting" },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test", Option::test,
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::HelpEntry{ L"help",
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
				arger::EnumEntry{ L"abc", L"This is the description of option abc", Mode::abc },
				arger::EnumEntry{ L"def", L"This is the description of option def", Mode::def }
			},
			arger::Value{ L"def" }
		},
		arger::Description{ L"Example of an enum" }
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
			arger::EnumEntry{ L"a", L"This is [a] description", 0 },
			arger::EnumEntry{ L"b", L"This is [b] description", 1 }
		}, L"First Argument" },
		arger::Information{ L"Read-Help", L"This is a help-description only shown for read." }
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

Options for [test-setting]:
  get                           Get something in the test program. But this is just a demo
                                 description
  read
  set                           Set something in the test program. But this is just a demo
                                 description

Required arguments:
  -p, --path=<file-path>        This is some path option (Used for: get|set)

Optional arguments:
  -h, --help                    Print this help menu.
  --mode=<test-mode> [enum]     Example of an enum
                                - [abc]: This is the description of option abc
                                - [def]: This is the description of option def
                                Defaults to: ([def])
  -t, --test                    This is the description of the test flag.

Some Description                This is the indepth description.
```

```
$ ./bin.exe read -h
Usage: bin.exe read [options...] argument...

    Some test program

Positional Arguments for test-setting [read]:
  argument [enum]               First Argument [2x]
                                - [a]: This is [a] description
                                - [b]: This is [b] description

Optional arguments:
  -h, --help                    Print this help menu.
  --mode=<test-mode> [enum]     Example of an enum
                                - [abc]: This is the description of option abc
                                - [def]: This is the description of option def
                                Defaults to: ([def])
  -t, --test                    This is the description of the test flag.

Some Description                This is the indepth description.

Read-Help                       This is a help-description only shown for read.
```

And examples of potential errors:

```
$ ./bin.exe
Test-Setting missing.

Try 'bin.exe --help' for more information.
```
```
$ ./bin.exe set 50
Argument [path] is missing.

Try 'bin.exe --help' for more information.
```
```
$ ./bin.exe read -p 50 a b
Argument [path] not meant for test-setting [read].

Try 'bin.exe --help' for more information.
```
