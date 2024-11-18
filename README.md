# Argument Parsing for C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Small header-only library written in `C++20` to add simple argument parsing to C++. This library adds support for optional flags/arguments, as well as sub-commands (called `group`), and positional arguments. Grouping hereby describes the practice of having sub-commands as the corresponding next primary argument. An example for this would be `git`, where the first argument is the actual sub-command to be executed.

The simple idea is to define the argument-layout using `arger::Config`. The `parse(int argc, const char* const* argv, const arger::Config& config)` will then produce a `arger::Parsed` structure, which contains the final results.

## Using the library
This library is a header only library. Simply clone the repository, ensure that `./repos` is on the path (or at least that `<ustring/ustring.h>` can be resolved), and include `<arger/arger.h>`.

	$ git clone https://github.com/BjoernBoss/arger.git --recursive

## Configuration Options

There exist a set of configuration options, which can either be applied to optional arguments (`arger::Option`), to groups (`arger::Group`) or the general configuration (`arger::Config`).
The following configurations are defined:

```C++
/* general arger-configuration to be parsed */
arger::Config(std::wstring program, std::wstring version, const arger::IsConfig<arger::Config> auto&... configs);

/* general sub-group of options for a configuration/group
*	 Note: can only have sub-groups or positional arguments */
arger::Group(std::wstring name, std::wstring id);

/* general optional flag/payload */
arger::Option(std::wstring name);

/* description to the corresponding object */
arger::Description(std::wstring desc);

/* add help-string to the corresponding object */
arger::Help(std::wstring name, std::wstring text);

/* add a constraint to be executed if the corresponding object is selected via the arguments */
arger::Constraint(arger::Checker constraint);

/* add a minimum/maximum requirement [maximum=0 implies no maximum]
*	- [Option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
*	- [Otherwise]: constrains the number of positional arguments with a default of [min = max = number-of-positionals];
*		if greater than number of positional arguments, last type is used as catch-all */
arger::Require(size_t min, size_t max);

/* add an abbreviation character for an option to allow it to be accessible as, for example, -x */
arger::Abbreviation(wchar_t c);

/* add a payload to an option with a given name and of a given type */
arger::Payload(std::wstring name, arger::Type type);

/* add usage-constraints to let the corresponding options only be used by groups, which add them as usage (by default every group/argument can use all options) */
arger::Use(const auto&... options);

/* mark this flag as being the help-indicating flag, which triggers the help-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::HelpFlag();

/* mark this flag as being the version-indicating flag, which triggers the version-menu to be printed (prior to verifying the remainder of the argument structure) */
arger::VersionFlag();

/* setup the descriptive name for the sub-groups to be used (the default name is 'mode') */
arger::GroupName(std::wstring name);

/* add an additional positional argument to the configuration/group using the given name, type, and description
*	 Note: can only have sub-groups or positional arguments */
arger::Positional(std::wstring name, arger::Type type, std::wstring description);
```

## Common Command Line Mode

This simple example shows the configuration for a simple command line mode, without using sub-commands.
<br>
The following example configuration:

```C++
arger::Config config{ L"test.exe", L"1.0.1",
	arger::Help{ L"Some Description", L"This is the indepth description." },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test",
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::Option{ L"help",
		arger::Abbreviation{ L'h' },
		arger::HelpFlag{},
		arger::Description{ L"Print this help menu." }
	},
	arger::Option{ L"path",
		arger::Abbreviation{ L'p' },
		arger::Payload{ L"file-path", arger::Primitive::any },
		arger::Description{ L"This is some path option" },
		arger::Require{ 1, 0 }
	},
	arger::Option{ L"mode",
		arger::Payload{ L"test-mode",
			arger::Enum{
				{ L"abc", L"This is the description of option abc" },
				{ L"def", L"This is the description of option def" }
			}
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
$ ./test.exe --help
test.exe Version [1.0.1]

    Some test program

Usage: test.exe --path=<file-path> [options...] first second [third...]

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
  -t, --test                    This is the description of the test flag.

Some Description                This is the indepth description.
```

And examples of potential errors:

```
$ ./test.exe 51 false --path=/foo/bar
Argument must be less than 50

Try 'test.exe --help' for more information.
```
```
$ ./test.exe 12 true --mode=ghi -tp=/foo/bar -- test1 test2 --mode=50
Invalid enum for argument [mode] encountered.

Try 'test.exe --help' for more information.
```


## Sub-Command Line Mode

To setup the sub-command line mode, add all sub-commands as groups using `arger::Group`. The term used in the help-output for `groups` can be configured using `arger::GroupName`. The default is `mode`.
<br>
The following example configuration:

```C++
arger::Config config{ L"test.exe", L"1.0.1",
	arger::Help{ L"Some Description", L"This is the indepth description." },
	arger::GroupName{ L"test-setting" },
	arger::Description{ L"Some test program" },
	arger::Option{ L"test",
		arger::Abbreviation{ L't' },
		arger::Description{ L"This is the description of the test flag." }
	},
	arger::Option{ L"help",
		arger::Abbreviation{ L'h' },
		arger::HelpFlag{},
		arger::Description{ L"Print this help menu." }
	},
	arger::Option{ L"path",
		arger::Abbreviation{ L'p' },
		arger::Payload{ L"file-path", arger::Primitive::any },
		arger::Description{ L"This is some path option" },
		arger::Require{ 1 }
	},
	arger::Option{ L"mode",
		arger::Payload{ L"test-mode",
			arger::Enum{
				{ L"abc", L"This is the description of option abc" },
				{ L"def", L"This is the description of option def" }
			}
		},
		arger::Description{ L"Example of an enum" }
	},
	arger::Group{ L"get", L"",
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
		arger::Use{ L"path" }
	},

	arger::Group{ L"set", L"",
		arger::Description{ L"Set something in the test program. But this is just a demo description" },
		arger::Require{ 1 },
		arger::Positional{ L"first", arger::Primitive::unum, L"First Argument" },
		arger::Use{ L"path" }
	},
	arger::Group{ L"read", L"",
		arger::Require{ 2 },
		arger::Positional{ L"argument", arger::Enum{
			{ L"a", L"This is [a] description" },
			{ L"b", L"This is [b] description" }
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
$ ./test.exe --help
test.exe Version [1.0.1]

    Some test program

Usage: test.exe [test-setting] --path=<file-path> [options...] [params...]

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
  -t, --test                    This is the description of the test flag.

Some Description                This is the indepth description.
```

```
$ ./test.exe read -h
test.exe Version [1.0.1]

    Some test program

Usage: test.exe read [options...] argument...

Positional Arguments for test-setting [read]:
  argument [enum]               First Argument [2x]
                                - [a]: This is [a] description
                                - [b]: This is [b] description

Optional arguments:
  -h, --help                    Print this help menu.
  --mode=<test-mode> [enum]     Example of an enum
                                - [abc]: This is the description of option abc
                                - [def]: This is the description of option def
  -t, --test                    This is the description of the test flag.

Some Description                This is the indepth description.

Read-Help                       This is a help-description only shown for read.
```

And examples of potential errors:

```
$ ./test.exe
Test-Setting missing.

Try 'test.exe --help' for more information.
```
```
$ ./test.exe set 50
Argument [path] is missing.

Try 'test.exe --help' for more information.
```
```
$ ./test.exe read -p 50 a b
Argument [path] not meant for test-setting [read].

Try 'test.exe --help' for more information.
```
