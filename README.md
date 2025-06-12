# Argument Parsing for C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Small header-only library written in `C++20` to add simple argument parsing to C++. This library adds support for optional flags/arguments, as well as sub-commands (called `group`), and positional arguments. Grouping hereby describes the practice of having sub-commands as the corresponding next primary argument. An example for this would be `git`, where the first argument is the actual sub-command to be executed.

The simple idea is to define the argument-layout using `arger::Config`. The `arger::Parse(int argc, const argv, const arger::Config& config)` method will then produce a `arger::Parsed` structure, which contains the final results.

For convenience, there also exists `arger::Menu(int argc, const argv, const arger::Config& config)`, which is designed to be used in command-line style menus. It will therefore not require, nor print any program information and redesign the help menu to fit a command-line style menu. It will also turn the help and version entries from options to group keys, which can be used at any time.

## Using the library
This library is a header only library. Simply clone the repository, ensure that `./repos` is on the path (or at least that `<ustring/ustring.h>` can be resolved), and include `<arger/arger.h>`.

	$ git clone https://github.com/BjoernBoss/arger.git --recursive

## Configuration Options

There exist a set of configuration options, which can either be applied to optional arguments (`arger::Option`), to groups (`arger::Group`) or the general configuration (`arger::Config`).
The following configurations are defined:

```C++
/* general arger-configuration to be parsed */
arger::Config(const arger::IsConfig<arger::Config> auto&... configs);

/* general sub-group of options for a configuration/group (id can be enum or int)
*	 Note: Groups/Configs can can only have sub-groups or positional arguments */
arger::Group(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Group> auto&... configs);

/* endpoint for positional arguments for a configuration/group (to enable a group to have multiple variations of positional counts)
*	 Note: If Groups/Configs define positional arguments directly, an implicit endpoint is defined and no further endpoints can be added */
arger::Endpoint(arger::IsId auto id, const arger::IsConfig<arger::Endpoint> auto&... configs);

/* general optional flag/payload (id can be enum or int)
*	Note: if passed to a group, it is implicitly only bound to that group - but all names and abbreviations must be unique */
arger::Option(std::wstring name, arger::IsId auto id, const arger::IsConfig<arger::Option> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::HelpEntry(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs);

/* configure the key to be used as option for argument mode and any group name for menu mode, which triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::VersionEntry(std::wstring name, const arger::IsConfig<arger::Option> auto&... configs);

/* version for the current configuration */
arger::Version(std::wstring version);

/* default alternative program name for the configuration */
arger::Program(std::wstring program);

/* description to the corresponding object (all children only applies to optional descriptions; configures if the text should be printed for all subsequent children as well) */
arger::Description(std::wstring desc, bool allChildren = true);

/* add help-string to the corresponding object (all children configures if the text should be printed for all subsequent children as well) */
arger::Help(std::wstring name, std::wstring text, bool allChildren = true);

/* add a constraint to be executed if the corresponding object is selected via the arguments */
arger::Constraint(arger::Checker constraint);

/* add a minimum/maximum requirement [maximum=0 implies no maximum]
*	- [Option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
*		if greater than number of positional arguments, last type is used as catch-all */
arger::Require(size_t min, size_t max);

/* add an abbreviation character for an option, group, or help/version entry to allow it to be accessible as single letters or, for example, -x */
arger::Abbreviation(wchar_t c);

/* set the endpoint id of the implicitly defined endpoint
*	Note: cannot be used in conjunction with explicitly defined endpoints */
arger::EndpointId(arger::IsId auto id);

/* add a payload to an option with a given name and of a given type, and optional default values (must meet the requirement-counts) */
arger::Payload(std::wstring name, arger::Type type, arger::Value defValue);
arger::Payload(std::wstring name, arger::Type type, std::vector<arger::Value> defValue = {});

/* add usage-constraints to let the corresponding options only be used by groups, which add them as usage (by default every group/argument can use all options) */
arger::Use(arger::IsId auto... options);

/* mark this flag/group as being the help-indicating flag, which triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::HelpFlag();

/* mark this flag as being the version-indicating flag, which triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::VersionFlag();

/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
arger::GroupName(std::wstring name);

/* add an additional positional argument to the configuration/group using the given name, type, description, and optional default value (must meet the requirement-counts)
*	Note: Groups/Configs can can only have sub-groups or positional arguments
*	Note: Default values will be used, when no argument is given, or the argument string is empty */
arger::Positional(std::wstring name, arger::Type type, std::wstring description);
arger::Positional(std::wstring name, arger::Type type, std::wstring description, arger::Value defValue);
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
	arger::Version{ L"1.0.1" },
	arger::Help{ L"Some Description", L"This is the indepth description." },
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
	arger::Version{ L"1.0.1" },
	arger::Help{ L"Some Description", L"This is the indepth description." },
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
		arger::Help{ L"Read-Help", L"This is a help-description only shown for read." }
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
