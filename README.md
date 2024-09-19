# Argument Parsing for C++
![C++](https://img.shields.io/badge/language-c%2B%2B20-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-brightgreen?style=flat-square)](LICENSE.txt)

Small library written in `C++20` to add simple argument parsing to C++. This library offers two kinds of argument structures, both of which use the common unix practices of argument parsing. The common command line mode, and the `group` mode. Grouping hereby describes the practice of having sub-commands as the first primary argument. An example for this would be `git`, where the first argument is the actual sub-command to be executed.

The simple idea is to define the argument-layout using `arger::Arguments`. Its `parse(int argc, const char* const* argv)` will then produce a `arger::Parsed` structure, which contains the final results.

## Using the library
This library only consists of two files. Simply clone the repository, ensure that `./repos` is on the path (or at least that `<ustring/ustring.h>` can be resolved), ensure that [arger.cpp](arger.cpp) is included in the compilation, and include `<arger/arger.h>`. The only further requirement is, that the library is compiled using `C++20`.

	$ git clone https://github.com/BjoernBoss/arger.git

## Configuration Options

There exist a set of configuration options, which can either be applied to optional arguments (using `addOption`), to groups or the single command-description (using `addGroup`), or globally to the argument parser (using `addGlobal`). The following configurations are defined:

```C++
	/* add a description to [group|option|global]; can only be set once per entry */
	arger::config::Description(std::wstring description);

	/* add a minimum/maximum requirement [maximum=0 implies no maximum]; can only be set once per entry
	*	- [option]: are only acknowledged for non-flags with a default of [min: 0, max: 1]
	*	- [group]: constrains the number of positional arguments with a default of [min = max = number-of-positionals]; if greater than number of positional arguments, last type is used as catch-all */
	arger::config::Require(size_t min, size_t max);

	/* add an abbreviation character to [option] to allow it to be accessible as -x; can only be set once per entry */
	arger::config::Abbreviation(wchar_t c);

	/* add a payload to [option] with the given name and type; can only be set once per entry */
	arger::config::Payload(std::wstring name, arger::Type type);

	/* bind the [option] to the given set of sub-commands; can be set multiple times */
	arger::config::Bind(std::set<std::wstring> names);

	/* define a constraint for [group|option|global] to be executed if the given entry is defined; can be added multiple times */
	arger::config::Constraint(std::function<std::wstring(arger::Parsed)> fn);

	/* define this [option] to trigger the help-menu; can only be set once per entry; implicitly marks this to be a non-constrainable/non-bindable flag */
	arger::config::HelpFlag();
	
	/* define this [option] to trigger the version-menu; can only be set once per entry; implicitly marks this to be a non-constrainable/non-bindable flag */
	arger::config::VersionFlag();

	/* add another positional argument to [group] in order; can be added multiple times */
	arger::config::Positional(std::wstring name, arger::Type type, std::wstring description);

	/* add a help subsection to [group|global] in order; can be added multiple times */
	arger::config::Help(std::wstring name, std::wstring description);
```

## Common Command Line Mode

To setup the common command line mode, simply only add a single group using `addGroup` with an empty name.
<br>
The following example configuration:

```C++
	namespace config = arger::config;

	arger::Arguments args{ L"1.0.1" };

	args.addGlobal(
		config::Help{ L"Some Description", L"This is the indepth description." } |
		config::Description{ L"Some test program" }
	);
	args.addOption(L"test",
		config::Abbreviation(L't') |
		config::Description(L"This is the description of the test flag.")
	);
	args.addOption(L"help",
		config::Abbreviation(L'h') |
		config::HelpFlag() |
		config::Description(L"Print this help menu.")
	);
	args.addOption(L"path",
		config::Abbreviation(L'p') |
		config::Payload(L"file-path", arger::Primitive::any) |
		config::Description(L"This is some path option") |
		config::Require(1, 0)
	);
	args.addOption(L"mode",
		config::Payload(L"test-mode", arger::Enum{
			{ L"abc", L"This is the description of option abc" },
			{ L"def", L"This is the description of option def" }
			}) |
		config::Description(L"Example of an enum")
	);
	args.addGroup(L"",
		config::Require(2, 0) |
		config::Positional(L"first", arger::Primitive::unum, L"First Argument") |
		config::Positional(L"second", arger::Primitive::boolean, L"Second Argument") |
		config::Positional(L"third", arger::Primitive::any, L"Third Argument") |
		config::Constraint([](const arger::Parsed& p) {
			if (p.positional(0)->unum() >= 50)
				return L"Argument must be less than 50";
			return L"";
		})
	);

	try {
		arger::Parsed parsed = args.parse(argc, argv);
	}
	catch (const arger::ParsingException& err) {
		str::PrintLn(err.what(), L"\n\n", arger::HelpHint(argc, argv));
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

To setup the sub-command line mode, add all sub-commands as groups using `addGroup` with non-empty names (there cannot be an empty name). The term used in the help-output for `groups` can be configured using the second argument of the constructor. The default is `mode`.
<br>
The following example configuration:

```C++
	namespace config = arger::config;

	arger::Arguments args{ L"1.0.1", L"test-setting" };

	args.addGlobal(
		config::Help{ L"Some Description", L"This is the indepth description." } |
		config::Description{ L"Some group program" }
	);
	args.addOption(L"test",
		config::Abbreviation(L't') |
		config::Description(L"This is the description of the test flag.")
	);
	args.addOption(L"help",
		config::Abbreviation(L'h') |
		config::HelpFlag() |
		config::Description(L"Print this help menu.")
	);
	args.addOption(L"path",
		config::Abbreviation(L'p') |
		config::Payload(L"file-path", arger::Primitive::any) |
		config::Description(L"This is some path option") |
		config::Require(1) |
		config::Bind({ L"get", L"set" })
	);
	args.addOption(L"mode",
		config::Payload(L"test-mode", arger::Enum{
			{ L"abc", L"This is the description of option abc" },
			{ L"def", L"This is the description of option def" }
			}) |
		config::Description(L"Example of an enum")
	);
	args.addGroup(L"get",
		config::Description(L"Get something in the test program. But this is just a demo description") |
		config::Require(2, 4) |
		config::Positional(L"first", arger::Primitive::unum, L"First Argument") |
		config::Positional(L"second", arger::Primitive::boolean, L"Second Argument") |
		config::Positional(L"third", arger::Primitive::any, L"Third Argument") |
		config::Constraint([](const arger::Parsed& p) {
			if (p.positional(0)->unum() >= 50)
				return L"Argument must be less than 50";
			return L"";
		})
	);
	args.addGroup(L"set",
		config::Description(L"Set something in the test program. But this is just a demo description") |
		config::Require(1) |
		config::Positional(L"first", arger::Primitive::unum, L"First Argument")
	);
	args.addGroup(L"read",
		config::Require(2) |
		config::Positional(L"argument", arger::Enum{
			{ L"a", L"This is [a] description" },
			{ L"b", L"This is [b] description" }
		}, L"First Argument") | 
		config::Help(L"Read-Help", L"This is a help-description only shown for read.")
	);

	try {
		arger::Parsed parsed = args.parse(argc, argv);
	}
	catch (const arger::ParsingException& err) {
		str::PrintLn(err.what(), L"\n\n", arger::HelpHint(argc, argv));
	}
	catch (const arger::PrintMessage& msg) {
		str::PrintLn(msg.what());
	}
```

Constructs the following help-page:
```
	$ ./test.exe --help
	test.exe Version [1.0.1]

	    Some group program

	Usage: test.exe test-setting --path=<file-path> [options...] [params...]

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

	    Some group program

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
