/**
# libforth.c.md
@file       libforth.c
@author     Richard James Howe.
@copyright  Copyright 2015,2016 Richard James Howe.
@license    MIT
@email      howe.r.j.89@gmail.com

@brief      A FORTH library, written in a literate style.

@todo Bias errnos so they are outside of the range reserved by Forth

## License

The MIT License (MIT)

Copyright (c) 2016 Richard James Howe

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

## Introduction

This file implements the core Forth interpreter, it is written in portable
C99. The file contains a virtual machine that can interpret threaded Forth
code and a simple compiler for the virtual machine, which is one of its
instructions. The interpreter can be embedded in another application and
there should be no problem instantiating multiple instances of the
interpreter.

For more information about Forth see:

* <https://en.wikipedia.org/wiki/Forth_%28programming_language%29>
* Thinking Forth by Leo Brodie
* Starting Forth by Leo Brodie

A glossary of words for FIG FORTH 79:

* <http://www.dwheeler.com/6502/fig-forth-glossary.txt>

And the more recent and widespread standard for ANS Forth:

* <http://lars.nocrew.org/dpans/dpans.htm>

The antecedent of this interpreter:

* <http://www.ioccc.org/1992/buzzard.2.c>

cxxforth, a literate Forth written in C++

* <https://github.com/kristopherjohnson/cxxforth>

Jones Forth, a literate Forth written in x86 assembly:

* <https://rwmj.wordpress.com/2010/08/07/jonesforth-git-repository/>
* <https://github.com/AlexandreAbreu/jonesforth> (backup)

A Forth processor:

* <http://www.excamera.com/sphinx/fpga-j1.html>

And my Forth processor based on this one:

* <https://github.com/howerj/fyp>

The repository should also contain:

* "readme.md"  : a Forth manual, and generic project information
* "forth.fth"  : basic Forth routines and startup code
* "libforth.h" : The header contains the API documentation

The structure of this file is as follows:

1) Headers and configuration macros
2) Enumerations and constants
3) Helping functions for the compiler
4) API related functions and Initialization code
5) The Forth virtual machine itself
6) An example main function called **main_forth** and support functions

Each section will be explained in detail as it is encountered.

An attempt has been made to make this document flow, as both a source
code document and as a description of how the Forth kernel works.
This is helped by the fact that the program is small and compact
without being written in obfuscated C. It is, as mentioned, compact,
and can be difficult to understand regardless of code quality. Some
of the semantics of Forth will not be familiar to C programmers.

A basic understanding of how to use Forth would help as this document is
meant to describe how a Forth implementation works and not as an
introduction to the language. A quote about the language from Wikipedia
best sums the language up:

	"Forth is an imperative stack-based computer programming language
	and programming environment.

	Language features include structured programming, reflection (the
	ability to modify the program structure during program execution),
	concatenative programming (functions are composed with juxtaposition)
	and extensibility (the programmer can create new commands).

	...

	A procedural programming language without type checking, Forth features
	both interactive execution of commands (making it suitable as a shell
	for systems that lack a more formal operating system) and the ability
	to compile sequences of commands for later execution."

Forth has a philosophy like most languages, one of simplicity, compactness
and of trying only to solve the problem at hand, even going as far as to try
to simplify the problem or replace the problem (which may span multiple
domains, not just software) with a simpler one. This is often not
a realistic way of tackling things and Forth has fallen out of
favor, it is nonetheless an interesting language which can be
implemented and understood by a single programmer (another integral part
of the Forth philosophy).

The core of the concept of the language - simplicity I would say - is
achieved by the following:

1) The language uses Reverse Polish Notation to enter expressions and parsing
is simplified to the extreme with space delimited words and numbers being
the most complex terms. This means a abstract syntax tree does not need to
be constructed and terms can be executed as soon as they are parsed. The
*parser* can described in only a handful of lines of C.
2) The language uses concatenation of Forth words (called functions in
other language) to create new words, this allows for small programs to
be created and encourages *factoring* definitions into smaller words.
3) The language is untyped.
4) Forth functions, or words, take their arguments implicitly and return
variables implicitly via a variable stack which the programmer explicitly
interacts with. A comparison of two languages behavior best illustrates the
point, we will define a function in C and in Forth that simply doubles a
number. In C this would be:

	int double_number(int x)
	{
		return x << 1;
	}

And in Forth it would be:

	: 2* 1 lshift ;

No types are needed, and the arguments and the return values are not
stated, unlike in C. Although this has the advantage of brevity, it is now
up to the programmer to manages those variables.

5) The input and output facilities are set up and used implicitly as well.
Input is taken from **stdin** and output goes to **stdout**, by default. 
Words that deal with I/O uses these file steams internally.
6) Error handling is traditionally non existent or limited.
7) This point is not a property of the language, but part of the way the
Forth programmer must program. The programmer must make their factored word
definitions *flow*. Instead of reordering the contents of the stack for
each word, words should be made so that the reordering does not have to
take place (ie. Manually performing the job of a optimizing compile another
common theme in Forth, this time with memory reordering).

The implicit behavior relating to argument passing and I/O really reduce
program size, the type of implicit behavior built into a language can
really define what that language is good for. For example AWK is naturally
good for processing text, thanks in large part to sensible defaults for how
text is split up into lines and records, and how input and output is
already set up for the programmer.

An example of this succinctness in AWK is the following program, which
can be typed in at the command line. It will read from the standard
input if no files are given, and print any lines longer than eighty characters
along with the line number of that line:

	awk '{line++}length > 80 {printf "%04u: %s\n", line, $0}' file.txt ...

For more information about AWK see:

* <http://www.grymoire.com/Unix/Awk.html>
* <https://en.wikipedia.org/wiki/AWK>
* <http://www.pement.org/awk/awk1line.txt>

Forth likewise can achieve succinctness and brevity because of its implicit
behavior.

Naturally we try to adhere to Forth philosophy, but also to Unix philosophy
(which most Forths do not do), this is described later on.

Glossary of Terms:

	VM             - Virtual Machine
	Cell           - The Virtual Machines natural Word Size, on a 32 bit
		       machine the Cell will be 32 bits wide
	Word           - In Forth a Word refers to a function, and not the
		       usual meaning of an integer that is the same size as
		       the machines underlying word size, this can cause confusion
	API            - Application Program Interface
	interpreter    - as in byte code interpreter, synonymous with virtual
		       machine.
	REPL           - Read-Evaluate-Print-Loop, this Forth actually provides
		       something more like a "REL", or Read-Evaluate-Loop (as printing
		       has to be done explicitly), but the interpreter is interactive
		       which is the important point
	RPN            - Reverse Polish Notation (see
		       <https://en.wikipedia.org/wiki/Reverse_Polish_notation>).
		       The Forth interpreter uses RPN to enter expressions.
	The stack      - Forth implementations have at least two stacks, one for
		       storing variables and another for control flow and temporary
		       variables, when the term *stack* is used on its own and with
		       no other context it refers to the *variable stack* and not
		       the *return stack*. This *variable stack* is used for
		       passing parameters into and return values to functions.
	Return stack   - Most programming languages have a call stack, C has one
		       but not one that the programmer can directly access, in
		       Forth manipulating the return stack is often used.
	factor         - factoring is splitting words into smaller words that
		       perform a specific function. To say a word is a natural
		       factor of another word is to say that it makes sense to take
		       some functionality of the word to be factored and to create
		       a new word that encapsulates that functionality. Forth
		       encourages heavy factoring of definitions.
	Command mode   - This mode executes both compiling words and immediate
		       words as they are encountered
	Compile mode   - This mode executes immediate words as they are
		       encountered, but compiling words are compiled into the
		       dictionary.
	Primitive      - A word whose instruction is built into the VM.
**/

/**
## Headers and configurations macros 
**/

/** 
This file implements a Forth library, so a Forth interpreter can be embedded
in another application, as such a subset of the functions in this file are
exported, and are documented in the *libforth.h* header 
**/
#include "libforth.h"

/**
We try to make good use of the C library as even microcontrollers have enough
space for a reasonable implementation of it, although it might require some
setup. The only time allocations are explicitly done is when the virtual
machine image is initialized, after this the VM does not allocate any
more memory.
**/
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

/*
#ifdef USE_BUILT_IN_CORE
unsigned char forth_core_data[] __attribute__((aligned(0x1000)));
forth_cell_t forth_core_size = DEFAULT_CORE_SIZE;
#endif
*/
/**
Traditionally Forth implementations were the only program running on the
(micro)computer, running on processors orders of magnitude slower than
this one, as such checks to make sure memory access was in bounds did not
make sense and the implementation had to have access to the entire machines
limited memory.

To aide debugging and to help ensure correctness the **ck** macro, a wrapper
around the function **check_bounds**, is called for most memory accesses that
the virtual machine makes.
**/
#ifndef NDEBUG
/**
@brief This is a wrapper around **check_bounds**, so we do not have to keep
typing in the line number, as so the name is shorter (and hence the checks
are out of the way visually when reading the code).

@param C expression to bounds check
@return check index 
**/
#define ck(C) check_bounds(o, &on_error, (C), __LINE__, o->core_size)
/**
@brief This is a wrapper around **check_bounds**, so we do not have to keep
typing in the line number, as so the name is shorter (and hence the checks
are out of the way visually when reading the code). This will check
character pointers instead of cell pointers, like **ck** does.
@param C expression to bounds check
@return checked character index 
**/
#define ckchar(C) check_bounds(o, &on_error, (C), __LINE__, \
			o->core_size * sizeof(forth_cell_t))
/**
@brief This is a wrapper around **check_depth**, to make checking the depth 
short and simple.
@param DEPTH current depth of the stack
**/
#define cd(DEPTH) check_depth(o, &on_error, S, (DEPTH), __LINE__)
/**
@brief This macro makes sure any dictionary pointers never cross into 
the stack area.
@param DPTR a index into the dictionary
@return checked index 
**/
#define dic(DPTR) check_dictionary(o, &on_error, (DPTR))
/**
@brief This macro wraps up the tracing function, which we may want to remove.
@param ENV forth environment
@param STK stack pointer
@param EXPECTED expected stack value
@param TOP current top of stack to print out
**/
#define TRACE(ENV,INSTRUCTION,STK,TOP) trace(ENV,INSTRUCTION,STK,TOP)
#else

/**
The following are defined only if we remove the checking and
the debug code.
**/

#define ck(C) (C)
#define ckchar(C) (C)
#define cd(DEPTH) ((void)DEPTH)
#define dic(DPTR) check_dictionary(o, &on_error, (DPTR))
#define TRACE(ENV, INSTRUCTION, STK, TOP)
#endif

/**
@brief When we are reading input to be parsed we need a space to hold that
input, the offset to this area is into a field called **m** in **struct forth**,
defined later, the offset is a multiple of cells and not chars.  
**/
#define STRING_OFFSET       (32u)

/**
@brief This defines the maximum length of a Forth words name, that is the
string that represents a Forth word, this number is in cells (or machine
words). 
**/
#define MAXIMUM_WORD_LENGTH (32u)

/**
@brief The minimum stack size of both the variable and return stack, the stack
size should not be made smaller than this otherwise the built in code and
code in *forth.fth* will not work.
**/
#define MINIMUM_STACK_SIZE  (64u)

/** 
@brief The start of the dictionary is after the registers and the 
**STRING_OFFSET**, this is the area where Forth definitions are placed. 

@note The string offset could be placed after the end of the dictionary
to save space, in the area between the end of the dictionary and the
beginning of the pad area.
**/
#define DICTIONARY_START (STRING_OFFSET+MAXIMUM_WORD_LENGTH/sizeof(forth_cell_t)) 

/**
Later we will encounter a field called **MISC**, a field in every Word
definition and is always present in the Words header. This field contains
multiple values at different bit offsets, only the lower 16 bits of this
cell are ever used. The next macros are helper to extract information from
the **MISC** field. 
**/

/**
@brief The bit offset for word length start.
**/
#define WORD_LENGTH_OFFSET  (8)  

/**
@brief **WORD_LENGTH** extracts the length of a Forth words name so we know
where it is relative to the **PWD** field of a word.
@param MISC This should be the **MISC** field of a word 
**/
#define WORD_LENGTH(MISC) (((MISC) >> WORD_LENGTH_OFFSET) & 0xff)

/**
@brief Test if a word is a **hidden** word, one that is not in the search
order for the dictionary.
@param **PWD** field to test
**/
#define WORD_HIDDEN(MISC) ((MISC) & 0x80)

/**
@brief The lower 7 bits of the MISC field are used for the VM instruction,
limiting the number of instructions the virtual machine can have in it, the
higher bits are used for other purposes.
**/
#define INSTRUCTION_MASK    (0x7f)

/**
@brief A mask that the VM uses to extract the instruction.
@param k This **MISC**, or a **CODE** Field of a Forth word 
**/
#define instruction(k)      ((k) & INSTRUCTION_MASK)

/**
@brief **F_VERIFY** is our assert macro that will always been defined 
regardless of whether **NDEBUG** is defined.
@param X expression to verify
**/
#define F_VERIFY(X)           do { if(!(X)) { abort(); } } while(0)

/** 
## Enumerations and Constants
**/

/**
This following string is a forth program that gets called when creating a
new Forth environment, it is not actually the first program that gets
run, but it is run before the user gets a chance to do anything.

The program is kept as small as possible, but is dependent on the virtual
machine image being set up correctly with other, basic, words being defined
first, they will be described as they are encountered. Suffice to say,
before this program is executed the following happens:

  1) The virtual machine image is initialized
  2) All the virtual machine primitives are defined
  3) All registers are named and some constants defined
  4) **;** is defined

Of note, words such as **if**, **else**, **then**, and even comments 
- **(** -, are not actually Forth primitives, there are defined in terms 
of other Forth words.

The Forth interpreter is a simple loop that does the following:

	Start the interpreter loop <-----------<-----------------<---.
	Get a space delimited word                                    \
	Attempt to look up that word in the dictionary                 \
	Was the word found?                                             ^
	|-Yes:                                                          |
	|   Are we in compile mode?                                     |
	|   |-Yes:                                                      ^
	|   | \-Is the Word an Immediate word?                          |
	|   |   |-Yes:                                                  |
	|   |   | \-Execute the word >--------->----------------->----->.
	|   |   \-No:                                                   |
	|   |     \-Compile the word into the dictionary >------->----->.
	|   \-No:                                                       |
	|     \-Execute the word >------------->----------------->----->.
	\-No:                                                           ^
	  \-Can the word be treated as a number?                        |
	    |-Yes:                                                      |
	    | \-Are we in compile mode?                                 |
	    |   |-Yes:                                                  |
	    |   | \-Compile a literal into the dictionary >------>----->.
	    |   \-No:                                                   |
	    |     \-Push the number to the variable stack >------>----->.
	    \-No:                                                       |
	      \-An Error has occurred, print out an error message >---->.

As you can see, there is not too much too it, however there are still a lot
of details left out, such as how exactly the virtual machine executes words
and how this loop is formed.

A short description of the words defined in **initial_forth_program**
follows, bear in mind that they depend on the built in primitives, the
named registers being defined, as well as **state** and **;**.

	here      - push the current dictionary pointer
	[         - immediately enter command mode
	]         - enter compile mode
	>mark     - make a hole in the dictionary and push a pointer to it
	:noname   - make an anonymous word definition, push token to it, the
	    definition is terminated by ';' like normal word definitions.
	if        - immediate word, begin if...else...then clause
	else      - immediate word, optional else clause
	then      - immediate word, end if...else...then clause
	begin     - immediate word, start a begin...until loop
	until     - immediate word, end begin...until loop, jump to matching
	    begin at run time if top of stack is zero.
	')'       - push a ")" character to the stack
	(         - begin a Forth comment, terminated by a )
	rot       - perform stack manipulation: x y z => y z x
	-rot      - perform stack manipulation: x y z => z x y
	tuck      - perform stack manipulation: x y   => y x y
	nip       - perform stack manipulation: x y   => y
	allot     - allocate space in the dictionary
	bl        - push the space character to the stack
	space     - print a space
	.         - print out current top of stack, followed by a space 
**/
static const char *initial_forth_program = 
": here h @ ; \n"
": [ immediate 0 state ! ; \n"
": ] 1 state ! ; \n"
": >mark here 0 , ; \n"
": :noname immediate -1 , here 2 , ] ; \n"
": if immediate ' ?branch , >mark ; \n"
": else immediate ' branch , >mark swap dup here swap - swap ! ; \n"
": then immediate dup here swap - swap ! ; \n"
": begin immediate here ; \n"
": until immediate ' ?branch , here - , ; \n"
": ')' 41 ; \n"
": ( immediate begin key ')' = until ; \n"
": rot >r swap r> swap ; \n"
": -rot rot rot ; \n"
": tuck swap over ; \n"
": nip swap drop ; \n"
": 2drop drop drop ; \n"
": allot here + h ! ; \n"
": bl 32 ; \n"
": emit _emit drop ; \n" 
": space bl emit ; \n"
": evaluate 0 evaluator ; \n"
": . pnum drop space ; \n"
": ? @ . ;\n" ;

/**
@brief This is a string used in number to string conversion in
**number_printer**, which is dependent on the current base.
**/
static const char conv[] = "0123456789abcdefghijklmnopqrstuvwxzy";

/**
@brief int to **char\*** map for file access methods.
**/
enum fams { 
	FAM_WO,   /**< write only */
	FAM_RO,   /**< read only */
	FAM_RW,   /**< read write */
	LAST_FAM  /**< marks last file access method */
};

/**
@brief These are the file access methods available for use when the virtual
machine is up and running, they are passed to the built in primitives that
deal with file input and output (such as open-file).

@note It might be worth adding more *fams*, which **fopen** can accept.
**/
static const char *fams[] = { 
	[FAM_WO] = "wb", 
	[FAM_RO] = "rb", 
	[FAM_RW] = "w+b", 
	NULL 
};

/**
@brief The following are different reactions errors can take when
using **longjmp** to a previous **setjump**.
**/
enum errors
{
	INITIALIZED, /**< setjmp returns zero if returning directly */
	OKAY,          /**< no error, do nothing */
	FATAL,       /**< fatal error, this invalidates the Forth image */
	RECOVERABLE, /**< recoverable error, this will reset the interpreter */
};

/**
We can serialize the Forth virtual machine image, saving it to disk so we
can load it again later. When saving the image to disk it is important
to be able to identify the file somehow, and to identify properties of 
the image.

Unfortunately each image is not portable to machines with different
cell sizes (determined by "sizeof(forth_cell_t)") and different endianess,
and it is not trivial to convert them due to implementation details.

**enum header** names all of the different fields in the header.

The first four fields (**MAGIC0**...**MAGIC3**) are magic numbers which identify
the file format, so utilities like *file* on Unix systems can differentiate
binary formats from each other.

**CELL_SIZE** is the size of the virtual machine cell used to create the image.

**VERSION** is used to both represent the version of the Forth interpreter and
the version of the file format.

**ENDIAN** is the endianess of the VM

**MAGIC7** is the last magic number.

When loading the image the magic numbers are checked as well as
compatibility between the saved image and the compiled Forth interpreter. 
**/
enum header { /**< Forth header description enum */
	MAGIC0,     /**< Magic number used to identify file type */
	MAGIC1,     /**< Magic number ... */
	MAGIC2,     /**< Magic number ... */
	MAGIC3,     /**< Magic number ...  */
	CELL_SIZE,  /**< Size of a Forth cell, or virtual machine word */
	VERSION,    /**< Version of the image */
	ENDIAN,     /**< Endianess of the interpreter */
	LOG2_SIZE,  /**< Log-2 of the size */
	MAX_HEADER_FIELD
};

/** 
The header itself, this will be copied into the **forth_t** structure on
initialization, the **ENDIAN** field is filled in then as it seems impossible
to determine the endianess of the target at compile time. 
**/
static const uint8_t header[MAX_HEADER_FIELD] = {
	[MAGIC0]    = 0xFF,
	[MAGIC1]    = '4',
	[MAGIC2]    = 'T',
	[MAGIC3]    = 'H',
	[CELL_SIZE] = sizeof(forth_cell_t),
	[VERSION]   = FORTH_CORE_VERSION,
	[ENDIAN]    = -1,
	[LOG2_SIZE]  = -1 
};

/**
@brief The main structure used by the virtual machine is **forth_t**.

The structure is defined here and not in the header to hide the implementation
details it, all API functions are passed an opaque pointer to the structure
(see <https://en.wikipedia.org/wiki/Opaque_pointer>).

Only three fields are serialized to the file saved to disk:

1) **header**

2) **core_size**

3) **m**

And they are done so in that order, **core_size** and **m** are save in 
whatever endianess the machine doing the saving is done in, however 
**core_size** is converted to a **uint64_t** before being save to disk 
so it is not of a variable size. **m** is a flexible array member 
**core_size** number of members.

The **m** field is the virtual machines working memory, it has its own internal
structure which includes registers, stacks and a dictionary of defined words.

The **m** field is laid out as follows, assuming the size of the virtual
machine is 32768 cells big:

	.-----------------------------------------------.
	| 0-3F      | 40-7BFF       |7C00-7DFF|7E00-7FFF|
	.-----------------------------------------------.
	| Registers | Dictionary... | V stack | R stack |
	.-----------------------------------------------.

	V stack = The Variable Stack
	R stack = The Return Stack

The dictionary has its own complex structure, and it always starts just
after the registers. It includes scratch areas for parsing words, start up
code and empty space yet to be consumed before the variable stack. The sizes
of the variable and returns stack change depending on the virtual machine
size. The structures within the dictionary will be described later on.

In the following structure, **struct forth**, values marked with a '~~'
are serialized, the serialization takes place in order. Values are written
out as they are. The size of the Forth memory core gets stored in the header,
the size must be a power of two, so its binary logarithm can be stored
in a single byte.

**/
struct forth { /**< FORTH environment */
	uint8_t header[sizeof(header)]; /**< ~~ header for core file */
	forth_cell_t core_size;  /**< size of VM */
	uint8_t *s;          /**< convenience pointer for string input buffer */
	char hex_fmt[16];    /**< calculated hex format */
	char word_fmt[16];   /**< calculated word format */
	forth_cell_t *S;     /**< stack pointer */
	forth_cell_t *vstart;/**< index into m[] where variable stack starts*/
	forth_cell_t *vend;  /**< index into m[] where variable stack ends*/
	const struct forth_functions *calls; /**< functions for CALL instruction */
	forth_cell_t *m;    /**< ~~ Forth Virtual Machine memory */
};

/**
@brief This enumeration describes the possible actions that can be taken when an
error occurs, by setting the right register value it is possible to make errors
halt the interpreter straight away, or even to make it invalidate the core.

This does not override the behavior of the virtual machine when it detects an
error that cannot be recovered from, only when it encounters an error such
as a divide by zero or a word not being found, not when the virtual machine
executes and invalid instruction (which should never normally happen unless
something has been corrupted).
**/
enum actions_on_error
{
	ERROR_RECOVER,    /**< recover when an error happens, like a call to ABORT */
	ERROR_HALT,       /**< halt on error */
	ERROR_INVALIDATE, /**< halt on error and invalid the Forth interpreter */
};

/**
@brief There are a small number of registers available to the virtual machine, 
they are actually indexes into the virtual machines main memory, this is so 
that the programs running on the virtual machine can access them. 

There are other registers that are in use that the virtual machine cannot 
access directly (such as the program counter or instruction pointer). 
Some of these registers correspond directly to well known Forth concepts, 
such as the dictionary and return stack pointers, others are just 
implementation details. 

X-Macros are an unusual but useful method of making tables
of data. We use this to store the registers name, it's address within
the virtual machine and the enumeration for it.

More information about X-Macros can be found here:

* <https://en.wikipedia.org/wiki/X_Macro>
* <http://www.drdobbs.com/cpp/the-x-macro/228700289>
* <https://stackoverflow.com/questions/6635851>

**/

#define XMACRO_REGISTERS \
 X("h",               DIC,            6,   "dictionary pointer")\
 X("r",               RSTK,           7,   "return stack pointer")\
 X("`state",          STATE,          8,   "interpreter state")\
 X("base",            BASE,           9,   "base conversion variable")\
 X("pwd",             PWD,            10,  "pointer to previous word")\
 X("`source-id",      SOURCE_ID,      11,  "input source selector")\
 X("`sin",            SIN,            12,  "string input pointer")\
 X("`sidx",           SIDX,           13,  "string input index")\
 X("`slen",           SLEN,           14,  "string input length")\
 X("`start-address",  START_ADDR,     15,  "pointer to start of VM")\
 X("`fin",            FIN,            16,  "file input pointer")\
 X("`fout",           FOUT,           17,  "file output pointer")\
 X("`stdin",          STDIN,          18,  "file pointer to stdin")\
 X("`stdout",         STDOUT,         19,  "file pointer to stdout")\
 X("`stderr",         STDERR,         20,  "file pointer to stderr")\
 X("`argc",           ARGC,           21,  "argument count")\
 X("`argv",           ARGV,           22,  "arguments")\
 X("`debug",          DEBUG,          23,  "turn debugging on/off if enabled")\
 X("`invalid",        INVALID,        24,  "non-zero on serious error")\
 X("`top",            TOP,            25,  "*stored* version of top of stack")\
 X("`instruction",    INSTRUCTION,    26,  "start up instruction")\
 X("`stack-size",     STACK_SIZE,     27,  "size of the stacks")\
 X("`error-handler",  ERROR_HANDLER,  28,  "actions to take on error")\
 X("`handler",        THROW,          29,  "exception handler is stored here")\
 X("`x",              SCRATCH_X,      30,  "scratch variable x")\
 X("`y",              SCRATCH_Y,      31,  "scratch variable y")

enum registers {  /**< virtual machine registers */
#define X(NAME, ENUM, VALUE, HELP) ENUM = VALUE,
	XMACRO_REGISTERS
#undef X
};

static const char *register_names[] = { /**< names of VM registers */
#define X(NAME, ENUM, VALUE, HELP) NAME,
	XMACRO_REGISTERS
#undef X
	NULL
};

/** 
@brief The enum **input_stream** lists values of the **SOURCE_ID** register.

Input in Forth systems traditionally (tradition is a word we will keep using
here, generally in the context of programming it means justification for
cruft) came from either one of two places, the keyboard that the programmer
was typing at, interactively, or from some kind of non volatile store, such
as a floppy disk. Our C program has no portable way of interacting
directly with the keyboard, instead it could interact with a file handle
such as **stdin**, or read from a string. This is what we do in this 
interpreter.

A word in Forth called **SOURCE-ID** can be used to query what the input device
currently is, the values expected are zero for interactive interpretation, or
minus one (minus one, or all bits set, is used to represent truth conditions
in most Forths, we are a bit more liberal in our definition of true) for string
input. These are the possible values that the **SOURCE_ID** register can take.
The **SOURCE-ID** word, defined in *forth.fth*, then does more processing
of this word.

Note that the meaning is slightly different in our Forth to what is meant
traditionally, just because this program is taking input from **stdin** (or
possibly another file handle), does not mean that this program is being
run interactively, it could possibly be part of a Unix pipe, which is
the reason the interpreter defaults to being as silent as possible.

**/
enum input_stream {
	FILE_IN,       /**< file input; this could be interactive input */
	STRING_IN = -1 /**< string input */
};

/** 
@brief **enum instructions** contains each virtual machine instruction, a valid
instruction is less than LAST. One of the core ideas of Forth is that
given a small set of primitives it is possible to build up a high level
language, given only these primitives it is possible to add conditional
statements, case statements, arrays and strings, even though they do not
exist as instructions here.

Most of these instructions are simple (such as; pop two items off the
variable stack, add them and push the result for **ADD**) however others are a
great deal more complex and will require paragraphs to explain fully
(such as **READ**, or how **IMMEDIATE** interacts with the virtual machines
execution). 

The instruction name, enumeration and a help string, are all stored with
an X-Macro. 

Some of these words are not necessary, that is they can be implemented in
Forth, but they are useful to have around when the interpreter starts
up for debugging purposes (like **pnum**).
 
**/

#define XMACRO_INSTRUCTIONS\
 X(PUSH,      "push",       " -- x : push a literal")\
 X(COMPILE,   "compile",    " -- : compile a pointer to a Forth word")\
 X(RUN,       "run",        " -- : run a Forth word")\
 X(DEFINE,    "define",     " -- : make new Forth word, set compile mode")\
 X(IMMEDIATE, "immediate",  " -- : make a Forth word immediate")\
 X(READ,      "read",       " c\" xxx\" -- : read in a Forth word and execute it")\
 X(LOAD,      "@",          "addr -- x : load a value")\
 X(STORE,     "!",          "x addr -- : store a value")\
 X(CLOAD,     "c@",         "c-addr -- x : load character value")\
 X(CSTORE,    "c!",         "x c-addr -- : store character value")\
 X(SUB,       "-",          "x1 x2 -- x3 : subtract x2 from x1 yielding x3")\
 X(ADD,       "+",          "x x -- x : add two values")\
 X(AND,       "and",        "x x -- x : bitwise and of two values")\
 X(OR,        "or",         "x x -- x : bitwise or of two values")\
 X(XOR,       "xor",        "x x -- x : bitwise exclusive or of two values")\
 X(INV,       "invert",     "x -- x : invert bits of value")\
 X(SHL,       "lshift",     "x1 x2 -- x3 : left shift x1 by x2")\
 X(SHR,       "rshift",     "x1 x2 -- x3 : right shift x1 by x2")\
 X(MUL,       "*",          "x x -- x : multiply to values")\
 X(DIV,       "/",          "x1 x2 -- x3 : divide x1 by x2 yielding x3")\
 X(ULESS,     "u<",         "x x -- bool : unsigned less than")\
 X(UMORE,     "u>",         "x x -- bool : unsigned greater than")\
 X(EXIT,      "exit",       " -- : return from a word defition")\
 X(KEY,       "key",        " -- char : get one character of input")\
 X(EMIT,      "_emit",      " char -- status : get one character of input")\
 X(FROMR,     "r>",         " -- x, R: x -- : move from return stack")\
 X(TOR,       ">r",         "x --, R: -- x : move to return stack")\
 X(BRANCH,    "branch",     " -- : unconditional branch")\
 X(QBRANCH,   "?branch",    "x -- : branch if x is zero")\
 X(PNUM,      "pnum",       "x -- : print a number")\
 X(QUOTE,     "'",          " -- addr : push address of word")\
 X(COMMA,     ",",          "x -- : write a value into the dictionary")\
 X(EQUAL,     "=",          "x x -- bool : compare two values for equality")\
 X(SWAP,      "swap",       "x1 x2 -- x2 x1 : swap two values")\
 X(DUP,       "dup",        "x -- x x : duplicate a value")\
 X(DROP,      "drop",       "x -- : drop a value")\
 X(OVER,      "over",       "x1 x2 -- x1 x2 x1 : copy over a value")\
 X(TAIL,      "tail",       " -- : tail recursion")\
 X(FIND,      "find",       "c\" xxx\" -- addr | 0 : find a Forth word")\
 X(DEPTH,     "depth",      " -- x : get current stack depth")\
 X(SPLOAD,    "sp@",        " -- addr : load current stack pointer ")\
 X(SPSTORE,   "sp!",        " addr -- : modify the stack pointer")\
 X(CLOCK,     "clock",      " -- x : DO NOT USE push a time value")\
 X(EVALUATOR, "evaluator", "c-addr u 0 | file-id 0 1 -- x : evaluate file/str")\
 X(PSTK,      ".s",         " -- : print out values on the stack")\
 X(RESTART,   "restart",    " error -- : restart system, cause error")\
 X(CALL,      "call",       "x1...xn c -- x1...xn c : call a function")\
 X(SYSTEM,    "system",     "DO NOT USE")\
 X(FCLOSE,    "close-file", "file-id -- ior : close a file")\
 X(FOPEN,     "open-file",  "c-addr u fam -- open a file")\
 X(FDELETE,   "delete-file",     "c-addr u -- : delete a file")\
 X(FREAD,     "read-file",       "c-addr u file-id -- u ior : write block")\
 X(FWRITE,    "write-file",      "c-addr u file-id -- u ior : read block")\
 X(FPOS,      "file-position",   "file-id -- u : get the file position")\
 X(FSEEK,     "reposition-file", "file-id u -- ior : reposition file")\
 X(FFLUSH,    "flush-file",      "file-id -- ior : flush a file")\
 X(FRENAME,   "rename-file",     "c-addr1 u1 c-addr2 u2 -- ior : rename file")\
 X(TMPFILE,   "temporary-file",  "DO NOT USE")\
 X(RAISE,     "raise",           "signal -- bool : raise a signal")\
 X(DATE,      "date",          " -- date : push the time")\
 X(MEMMOVE,   "memory-copy",   " r-addr1 r-addr2 u -- : move a block of memory")\
 X(MEMCHR,    "memory-locate", " r-addr char u -- r-addr | 0 : locate a character memory")\
 X(MEMSET,    "memory-set",    " r-addr char u -- : set a block of memory")\
 X(MEMCMP,    "memory-compare", " r-addr1 r-addr2 u -- x : compare two blocks of memory")\
 X(ALLOCATE,  "allocate",       " u -- r-addr ior : allocate a block of memory")\
 X(FREE,      "free",           " r-addr1 -- ior : free a block of memory")\
 X(RESIZE,    "resize",         " r-addr u -- r-addr ior : free a block of memory")\
 X(LAST_INSTRUCTION, NULL, "")

/**
@todo Add the following instructions:
 * memory-compare memory-copy
 * resize-core 
**/

enum instructions { /**< instruction enumerations */
#define X(ENUM, STRING, HELP) ENUM,
	XMACRO_INSTRUCTIONS
#undef X
};

/**
So that we can compile programs we need ways of referring to the basic
programming constructs provided by the virtual machine, theses words are
fed into the C function **compile** in a process described later.

**LAST_INSTRUCTION** is not an instruction, but only a marker of the last
enumeration used in **enum instructions**, so it does not get a name.
**/
static const char *instruction_names[] = { /**< instructions with names */
#define X(ENUM, STRING, HELP) STRING,
	XMACRO_INSTRUCTIONS
#undef X
};

/**
## Helping Functions For The Compiler
**/

const char *forth_strerror(void)
{
	static const char *unknown = "unknown reason";
	const char *r = errno ? strerror(errno) : unknown;
	if(!r) 
		r = unknown;
	return r;
}

int forth_logger(const char *prefix, const char *func, 
		unsigned line, const char *fmt, ...)
{
	int r;
	va_list ap;
	assert(prefix && func && fmt);
	fprintf(stderr, "[%s %u] %s: ", func, line, prefix);
	va_start(ap, fmt);
	r = vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	return r;
}

/**
@brief  Get a char from string input or a file
@param  o   forth image containing information about current input stream
@return int same value as fgetc or getchar

This Forth interpreter only has a limited number of mechanisms for I/O, one 
of these is to fetch an individual character of input from either a string 
or a file which can be set either with knowledge of the implementation 
from within the virtual machine, or via the API presented to the programmer. 

The C functions **forth_init**, **forth_set_file_input** and
**forth_set_string_input** set up and manipulate the input of the
interpreter. These functions act on the following registers:

	SOURCE_ID - The current input source (SIN or FIN)
	SIN       - String INput
	SIDX      - String InDeX
	SLEN      - String LENgth
	FIN       - File   INput

Note that either SIN or FIN might not both be valid, one will be but the
other might not, this makes manipulating these values hazardous. The input
functions **forth_get_char** and **forth_ge\t_word** both take their input
streams implicitly via the registers contained within the Forth execution
environment passed in to those functions.
**/
static int forth_get_char(forth_t *o)
{
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   return fgetc((FILE*)(o->m[FIN]));
	case STRING_IN: return o->m[SIDX] >= o->m[SLEN] ? 
				EOF : 
				((char*)(o->m[SIN]))[o->m[SIDX]++];
	default:        return EOF;
	}
}

/**
**/

int echo(char c, FILE *out)
{
	int r = 0;
	if(out) {
		r = fputc(c, out);
		fflush(out);
	}
	return r;
}

int echo_fgetc(FILE *in, FILE *out)
{
	int c = fgetc(in);
	echo(c, out);
	return c;
}

/**
@brief get a word (space delimited, up to 31 chars) from a FILE\* or string-in
@param o    initialized Forth environment.
@param p    pointer to string to write into
@return int return status of [fs]scanf

This function reads in a space delimited word, limited to 
**MAXIMUM_WORD_LENGTH**, the word is put into the pointer **\*p**, 
due to the simple nature of Forth this is as complex as parsing and 
lexing gets. It can either read from a file handle or a string, 
like forth_get_char() 
**/
static int forth_get_word(forth_t *o, uint8_t *p)
{
	int n = 0;
	int c;
	char buffer[32];
	FILE *in = (FILE*)(o->m[FIN]);
	FILE *out;
	switch(o->m[SOURCE_ID]) {
	case FILE_IN:   
		n = fscanf(in, o->word_fmt, p, &n);
		if(!((char*)o->s)[0]) /* empty word */
			return -1; /** @note this should throw -16 **/
		return n;
	case STRING_IN:
		if(sscanf((char *)&(((char*)(o->m[SIN]))[o->m[SIDX]]), 
					o->word_fmt, p, &n) <= 0)
			return EOF;
		if(!((char*)o->s)[0]) /* empty word */
			return -1;  /** @note this should throw -16 **/
		o->m[SIDX] += n;
		return n;
	default:       return EOF;
	}
}

/** 
@brief Compile a Forth word header into the dictionary
@param o    Forth environment to do the compilation in
@param code virtual machine instruction for that word
@param str  name of Forth word

The function **compile** is not that complicated in itself, however it
requires an understanding of the structure of a Forth word definition and
the behavior of the Forth run time.

In all Forth implementations there exists a concept of *the dictionary*,
although they may be implemented in different ways the usual way is as a
linked list of words, starting with the latest defined word and ending with
a special terminating value. Words cannot be arbitrarily deleted, deletions
have to occur in the reverse order that they are defined.

Each word or Forth function that has been defined can be looked up in this 
dictionary, and dependent on whether it is an immediate word or a compiling 
word, and whether we are in command or compile mode different actions are 
taken when we have found the word we are looking for in our Read-Evaluate-Loop.

	    | <-- Start of VM memory
	    |             | <-- Start of dictionary
	    |             |
	.------------.    |  .------.      .------.             .-------------.
	| Terminator | <---- | Word | <--- | Word | < -- ... -- | Latest Word |
	.------------.    |  .------.      .------.             .-------------.
	    |                                                     ^
	    |                                                     |
	    |                                                   PWD Register

The **PWD** registers points to the latest defined word, a search starts from
here and works it way backwards (allowing us replace old definitions by
appending new ones with the same name only), the terminator

Our word header looks like this:

	.-----------.-----.------.--------.------------.
	| Word Name | PWD | MISC | CODE-2 | Data Field |
	.-----------.-----.------.--------.------------.

* **CODE-2** and the **Data Field** are optional and the **Data Field** is of
variable length.
* **Word Name** is a variable length field whose size is recorded in the
MISC field.

And the **MISC** field is a composite to save space containing a virtual
machine instruction, the hidden bit and the length of the Word Name string
as an offset in cells from **PWD** field. The field looks like this:

	----.-------------------.------------.-------------.
	... | 16 ........... 8  |    9       | 7 ....... 0 |
	... |  Word Name Size   | Hidden Bit | Instruction |
	-----.-------------------.------------.-------------.

The maximum value for the Word Name field is determined by the width of
the Word Name Size field.

The hidden bit is not used in the **compile** function, but is used
elsewhere (in **forth_find**) to hide a word definition from the word
search. The hidden bit is not set within this program at all, however it
can be set by a running Forth virtual machine (and it is, if desired).

The **Instruction** tells the interpreter what to do with the Word
definition when it is found and how to interpret **CODE-2** and the
**Data Field** if they exist. 
**/
static void compile(forth_t *o, forth_cell_t code, const char *str)
{ 
	assert(o && code < LAST_INSTRUCTION);
	forth_cell_t *m = o->m, wheader = m[DIC], l = 0;
	/*FORTH header structure */
	/*Copy the new FORTH word into the new header */
	strcpy((char *)(o->m + wheader), str); 
	/* align up to size of cell */
	l = strlen(str) + 1;
	l = (l + (sizeof(forth_cell_t) - 1)) & ~(sizeof(forth_cell_t) - 1); 
	l = l/sizeof(forth_cell_t);
	m[DIC] += l; /* Add string length in words to header (STRLEN) */

	m[m[DIC]++] = m[PWD]; /*0 + STRLEN: Pointer to previous words header */
	m[PWD] = m[DIC] - 1;  /*Update the PWD register to new word */
	/*size of words name and code field*/
	m[m[DIC]++] = (l << WORD_LENGTH_OFFSET) | code; 
}

/**
@brief This function turns a string into a number using a base and 
returns an error code to indicate success or failure, the results of 
the conversion are stored in **n**, even if the conversion failed.
**/
int forth_string_to_cell(int base, forth_cell_t *n, const char *s)
{
	char *end = NULL;
	errno = 0;
	*n = strtol(s, &end, base);
	return errno || *s == '\0' || *end != '\0';
}

/** 
@brief Forths are usually case insensitive and are required to be (or
at least accept only uppercase characters only) by the majority of the
standards for Forth.  As an aside I do not believe case insensitivity is
a good idea as it complicates interfaces and creates as much confusion
as it tries to solve (not only that, but different case letters do convey
information). However, in keeping with other implementations, this Forth
is also made insensitive to case **DUP** is treated the same as **dup**
and **Dup**.

This comparison function, **istrcmp**, is only used in one place however,
in the C function **forth_find**, replacing it with **strcmp** will
bring back the more logical, case sensitive, behavior.

@param  a   first string to compare
@param  b   second string
@return int same as **strcmp**, only case insensitive

**/
static int istrcmp(const char *a, const char *b)
{
	for(; ((*a == *b) || (tolower(*a) == tolower(*b))) && *a && *b; a++, b++)
		;
	return tolower(*a) - tolower(*b);
}

/**
The **match** function returns true if the word is not hidden and if
a case sensitive case sensitive has succeeded.
**/
static int match(forth_cell_t *m, forth_cell_t pwd, const char *s)
{
	forth_cell_t len = WORD_LENGTH(m[pwd + 1]);
	return !WORD_HIDDEN(m[pwd+1]) && !istrcmp(s, (char*)(&m[pwd-len]));
}

/** 
**forth_find** finds a word in the dictionary and if it exists it returns a
pointer to its **PWD** field. If it is not found it will return zero, also of
notes is the fact that it will skip words that are hidden, that is the
hidden bit in the **MISC** field of a word is set. The structure of the
dictionary has already been explained, so there should be no surprises in
this word. Any improvements to the speed of this word would speed up the
text interpreter a lot, but not the virtual machine in general.
**/
forth_cell_t forth_find(forth_t *o, const char *s)
{
	forth_cell_t *m = o->m, pwd = m[PWD];
	for (;pwd > DICTIONARY_START && !match(m, pwd, s);)
		pwd = m[pwd];
	return pwd > DICTIONARY_START ? pwd + 1 : 0;
}

/**
@brief Print a number in a given base to an output stream
@param u    number to print
@param base base to print in (must be between 1 and 37)
@param out  output file stream
@return zero or positive on success, negative on failure 
**/
static int print_unsigned_number(forth_cell_t u, forth_cell_t base, FILE *out)
{
	assert(base > 1 && base < 37);
	int i = 0, r = 0;
	char s[64 + 1] = ""; 
	do 
		s[i++] = conv[u % base];
	while ((u /= base));
	for(; i >= 0 && r >= 0; i--)
		r = fputc(s[i], out);
	return r;
}

/**
@brief  Print out a forth cell as a number, the output base being determined
by the **BASE** registers:
 @param  o     an initialized forth environment, contains BASE
 @param  f     value to print out
 @return int   zero or positive on success, negative on failure 
**/
static int print_cell(forth_t *o, FILE *output, forth_cell_t f)
{
	unsigned base = o->m[BASE];
	if(base == 10 || base == 0)
		return fprintf(output, "%"PRIdCell, f);
	if(base == 16)
		return fprintf(output, o->hex_fmt, f);
	if(base == 1 || base > 36)
		return -1;
	return print_unsigned_number(f, base, output);
}

/**
**check_bounds** is used to both check that a memory access performed by
the virtual machine is within range and as a crude method of debugging the
interpreter (if it is enabled). The function is not called directly but is
instead wrapped in with the **ck** macro, it can be removed with
compile time defines, removing the check and the debugging code.
**/
static forth_cell_t check_bounds(forth_t *o, jmp_buf *on_error, 
		forth_cell_t f, unsigned line, forth_cell_t bound)
{
	if(o->m[DEBUG] >= FORTH_DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", f, line);
	if(f >= bound) {
		fatal("bounds check failed (%"PRIdCell" >= %zu) line %u", 
				f, (size_t)bound, line);
		longjmp(*on_error, FATAL);
	}
	return f;
}

/**
**check_depth** is used to check that there are enough values on the stack
before an operation takes place. It is wrapped up in the **cd** macro. 
**/
static void check_depth(forth_t *o, jmp_buf *on_error, 
		forth_cell_t *S, forth_cell_t expected, unsigned line)
{
	if(o->m[DEBUG] >= FORTH_DEBUG_CHECKS)
		debug("0x%"PRIxCell " %u", (forth_cell_t)(S - o->vstart), line);
	if((uintptr_t)(S - o->vstart) < expected) {
		error("stack underflow %p -> %u", S, line);
		longjmp(*on_error, RECOVERABLE);
	} else if(S > o->vend) {
		error("stack overflow %p -> %u", S - o->vend, line);
		longjmp(*on_error, RECOVERABLE);
	}
}

/**
Check that the dictionary pointer does not go into the stack area:
**/
static forth_cell_t check_dictionary(forth_t *o, jmp_buf *on_error, 
		forth_cell_t dptr)
{
	if((o->m + dptr) >= (o->vstart)) {
		fatal("dictionary pointer is in stack area %"PRIdCell, dptr);
		forth_invalidate(o);
		longjmp(*on_error, FATAL);
	}
	return dptr;
}

/**
This checks that a Forth string is *NUL* terminated, as required by most C
functions, which should be the last character in string (which is s+end).
There is a bit of a mismatch between Forth strings (which are pointer to
the string and a length) and C strings, which a pointer to the string and
are *NUL* terminated. This function helps to correct that.
**/
static void check_is_asciiz(jmp_buf *on_error, char *s, forth_cell_t end)
{
	if(*(s + end) != '\0') {
		error("not an ASCIIZ string at %p", s);
		longjmp(*on_error, RECOVERABLE);
	}
}

/**
This function gets a string off the Forth stack, checking that the string
is *NUL* terminated. It is a helper function used when a Forth string has to
be converted to a C string so it can be passed to a C function.
**/
static char *forth_get_string(forth_t *o, jmp_buf *on_error, 
		forth_cell_t **S, forth_cell_t f)
{
	forth_cell_t length = f + 1;
	char *string = ((char*)o->m) + **S;
	(*S)--;
	check_is_asciiz(on_error, string, length);
	return string;
}

/** 
Forth file access methods (or *fam*s) must be held in a single cell, this
requires a method of translation from this cell into a string that can be
used by the C function **fopen** 
**/
static const char* forth_get_fam(jmp_buf *on_error, forth_cell_t f)
{
	if(f >= LAST_FAM) {
		error("Invalid file access method %"PRIdCell, f);
		longjmp(*on_error, RECOVERABLE);
	}
	return fams[f];
}

/**
This prints out the Forth stack, which is useful for debugging. 
**/
static void print_stack(forth_t *o, FILE *out, forth_cell_t *S, forth_cell_t f)
{ 
        int j;
	forth_cell_t depth = (forth_cell_t)(S - o->vstart);
	fprintf(out, "%"PRIdCell": ", depth);
	if(!depth)
		return;
        forth_cell_t i = S - o->vstart - 1;
	for(j = 1; i; i--, j++) {
		print_cell(o, out, *(o->S + j + 1));
		fputc(' ', out);
	}
	print_cell(o, out, f);
	fputc(' ', out);
}

/**
This function allows for some more detailed tracing to take place, reading 
the logs is difficult, but it can provide *some* information about what
is going on in the environment. This function will be compiled out if 
**NDEBUG** is defined by the C preprocessor.
**/
static void trace(forth_t *o, forth_cell_t instruction, 
		forth_cell_t *S, forth_cell_t f)
{
	if(o->m[DEBUG] < FORTH_DEBUG_INSTRUCTION)
		return;
	if(instruction > LAST_INSTRUCTION) {
		error("traced invalid instruction %"PRIdCell, instruction);
		return;
	}
	fprintf(stderr, "\t( %s\t ", instruction_names[instruction]);
	print_stack(o, stderr, S, f);
	fputs(" )\n", stderr);
}

/** 
## API related functions and Initialization code 
**/

void forth_set_file_input(forth_t *o, FILE *in)
{
	assert(o);
	assert(in);
	o->m[SOURCE_ID] = FILE_IN;
	o->m[FIN]       = (forth_cell_t)in;
}

void forth_set_file_output(forth_t *o, FILE *out)
{
	assert(o && out);
	o->m[FOUT] = (forth_cell_t)out;
}

void forth_set_string_input(forth_t *o, const char *s)
{
	assert(o && s);
	o->m[SIDX] = 0;              /* m[SIDX] == current character in string */
	o->m[SLEN] = strlen(s) + 1;  /* m[SLEN] == string len */
	o->m[SOURCE_ID] = STRING_IN; /* read from string, not a file handle */
	o->m[SIN] = (forth_cell_t)s; /* sin  == pointer to string input */
}

int forth_eval(forth_t *o, const char *s)
{
	assert(o && s);
	forth_set_string_input(o, s);
	return forth_run(o);
}

int forth_define_constant(forth_t *o, const char *name, forth_cell_t c)
{
	char e[MAXIMUM_WORD_LENGTH+32] = {0};
	assert(o && strlen(name) < MAXIMUM_WORD_LENGTH);
	sprintf(e, ": %31s %" PRIdCell " ; \n", name, c);
	return forth_eval(o, e);
}

void forth_set_args(forth_t *o, int argc, char **argv)
{ /* currently this is of little use to the interpreter */
	assert(o);
	o->m[ARGC] = argc;
	o->m[ARGV] = (forth_cell_t)argv;
}

int forth_is_invalid(forth_t *o)
{
	assert(o);
	return !!(o->m[INVALID]);
}

void forth_invalidate(forth_t *o)
{
	assert(o);
	o->m[INVALID] = 1;
}

void forth_set_debug_level(forth_t *o, enum forth_debug_level level)
{
	o->m[DEBUG] = level;
}

FILE *forth_fopen_or_die(const char *name, char *mode)
{
	errno = 0;
	FILE *file = fopen(name, mode);
	if(!file) {
		fatal("opening file \"%s\" => %s", name, forth_strerror());
		exit(EXIT_FAILURE);
	}
	return file;
}

/**
@brief This function defaults all of the registers in a Forth environment
and sets up the input and output streams.

@param o     the forth environment to set up
@param size  the size of the **m** field in **o**
@param in    the input file
@param out   the output file

**forth_make_default** default is called by **forth_init** and
**forth_load_core_file**, it is a routine which deals that sets up registers for
the virtual machines memory, and especially with values that may only be
valid for a limited period (such as pointers to **stdin**). 
**/
static void forth_make_default(forth_t *o, size_t size, FILE *in, FILE *out)
{
	assert(o && size >= MINIMUM_CORE_SIZE && in && out);
	o->core_size     = size;
	o->m[STACK_SIZE] = size / MINIMUM_STACK_SIZE > MINIMUM_STACK_SIZE ?
				size / MINIMUM_STACK_SIZE :
				MINIMUM_STACK_SIZE;

	o->s             = (uint8_t*)(o->m + STRING_OFFSET); /*skip registers*/
	o->m[FOUT]       = (forth_cell_t)out;
	o->m[START_ADDR] = (forth_cell_t)&(o->m);
	o->m[STDIN]      = (forth_cell_t)stdin;
	o->m[STDOUT]     = (forth_cell_t)stdout;
	o->m[STDERR]     = (forth_cell_t)stderr;
	o->m[RSTK] = size - o->m[STACK_SIZE]; /* set up return stk ptr */
	o->m[ARGC] = o->m[ARGV] = 0;
	o->S       = o->m + size - (2 * o->m[STACK_SIZE]); /* v. stk pointer */
	o->vstart  = o->m + size - (2 * o->m[STACK_SIZE]);
	o->vend    = o->vstart + o->m[STACK_SIZE];
	F_VERIFY(sprintf(o->hex_fmt, "0x%%0%d"PRIxCell, 
				(int)sizeof(forth_cell_t)*2) > 0);
	F_VERIFY(sprintf(o->word_fmt, "%%%ds%%n", MAXIMUM_WORD_LENGTH - 1) > 0);
	forth_set_file_input(o, in);  /* set up input after our eval */
}

/**
@brief This function simply copies the current Forth header into a byte
array, filling in the endianess which can only be determined at run time.
@param dst a byte array at least "sizeof header" large 
**/
static void make_header(uint8_t *dst, uint8_t log2size)
{
	memcpy(dst, header, sizeof header);
	/*fill in endianess, needs to be done at run time */
	dst[ENDIAN] = !IS_BIG_ENDIAN;
	dst[LOG2_SIZE] = log2size;
}

/**
Calculates the binary logarithm of a forth cell, rounder up towards infinity.
This used for storing the size field in the header.
**/
forth_cell_t forth_blog2(forth_cell_t x)
{
	forth_cell_t b = 0;
	while(x >>= 1)
		b++;
	return b;
}

/**
This rounds up an integer to the nearest power of two larger than that integer.
**/
forth_cell_t forth_round_up_pow2(forth_cell_t r)
{
	forth_cell_t up = 1;
	while(up < r)
		up <<= 1;
	return up;
}

/**
**forth_init** is a complex function that returns a fully initialized forth
environment we can start executing Forth in, it does the usual task of
allocating memory for the object to be returned, but it also does has the
task of getting the object into a runnable state so we can pass it to
**forth_run** and do useful work. 
**/
forth_t *forth_init(size_t size, FILE *in, FILE *out, 
		const struct forth_functions *calls)
{
	assert(in && out);
	forth_cell_t *m, i, w, t, pow;
	forth_t *o;
	assert(sizeof(forth_cell_t) >= sizeof(uintptr_t));
	size = forth_round_up_pow2(size);
	pow  = forth_blog2(size);
/**
There is a minimum requirement on the **m** field in the **forth_t** structure
which is not apparent in its definition (and cannot be made apparent given
how flexible array members work). We need enough memory to store the registers
(32 cells), the parse area for a word (**MAXIMUM_WORD_LENGTH** cells), the 
initial start up program (about 6 cells), the initial built in and defined 
word set (about 600-700 cells) and the variable and return stacks 
(**MINIMUM_STACK_SIZE** cells each, as minimum).

If we add these together we come up with an absolute minimum, although
that would not allow us define new words or do anything useful. We use
**MINIMUM_STACK_SIZE** to define a useful minimum, albeit a restricted on, it
is not a minimum large enough to store all the definitions in *forth.fth*
(a file within the project containing a lot of Forth code) but it is large
enough for embedded systems, for testing the interpreter and for the unit
tests within the *unit.c* file.

We **F_VERIFY** that the size has been passed in is equal to or about minimum as
this has been documented as being a requirement to this function in the C API,
if we are passed a lower number the programmer has made a mistake somewhere
and should be informed of this problem.
**/
	F_VERIFY(size >= MINIMUM_CORE_SIZE);
	if(!(o = calloc(1, sizeof(*o) + sizeof(forth_cell_t)*size)))
		return NULL;

/** 
Default the registers, and input and output streams:
**/
	forth_make_default(o, size, in, out);

/** 
**o->header** needs setting up, but has no effect on the run time behavior of
the interpreter:
**/
	make_header(o->header, pow);

	o->calls = calls; /* pass over functions for CALL */
	m = o->m;         /* a local variable only for convenience */

/**
The next section creates a word that calls **READ**, then **TAIL**,
then itself. This is what the virtual machine will run at startup so
that we can start reading in and executing Forth code. It creates a
word that looks like this:

	| <-- start of dictionary          |
	.------.------.-----.----.----.----.
	| TAIL | READ | RUN | P1 | P2 | P2 | Rest of dictionary ...
	.------.------.-----.----.----.----.
	|     end of this special word --> |

	P1 is a pointer to READ
	P2 is a pointer to TAIL
	P2 is a pointer to RUN

The effect of this can be described as "make a function which
performs a **READ** then calls itself tail recursively". The first
instruction run is **RUN** which we save in **o->m[INSTRUCTION]** and
restore when we enter **forth_run**.
**/
	o->m[PWD]   = 0;  /* special terminating pwd value */
	t = m[DIC] = DICTIONARY_START; /* initial dictionary offset */
	m[m[DIC]++] = TAIL; /* add a TAIL instruction that can be called */
	w = m[DIC];         /* save current offset, which will contain READ */
	m[m[DIC]++] = READ; /* populate the cell with READ */
	m[m[DIC]++] = RUN;  /* call the special word recursively */
	o->m[INSTRUCTION] = m[DIC]; /* stream points to the special word */
	m[m[DIC]++] = w;    /* call to READ word */
	m[m[DIC]++] = t;    /* call to TAIL */
	m[m[DIC]++] = o->m[INSTRUCTION] - 1; /* recurse*/

/**
**DEFINE** and **IMMEDIATE** are two immediate words, the only two immediate
words that are also virtual machine instructions, we can make them
immediate by passing in their code word to **compile**. The created
word looks like this

	.------.-----.------.
	| NAME | PWD | MISC |
	.------.-----.------.

The **MISC** field here contains either **DEFINE** or **IMMEDIATE**, as well as
the hidden bit field and an offset to the beginning of name. 
**/
	compile(o, DEFINE,    ":");
	compile(o, IMMEDIATE, "immediate");

/**
All of the other built in words that use a virtual machine instruction to
do work are instead compiling words, and because there are lots of them we
can initialize them in a loop

The created word looks like this:

	.------.-----.------.----------------.
	| NAME | PWD | MISC | VM-INSTRUCTION |
	.------.-----.------.----------------.

The MISC field here contains the **COMPILE** instructions, which will compile a
pointer to the **VM-INSTRUCTION**, as well as the other fields it usually 
contains.
**/
	for(i = READ, w = READ; instruction_names[i]; i++) {
		compile(o, COMPILE, instruction_names[i]);
		m[m[DIC]++] = w++; /*This adds the actual VM instruction */
	}
/**
The next eval is the absolute minimum needed for a sane environment, it
defines two words **state** and **;**
**/
	F_VERIFY(forth_eval(o, ": state 8 exit : ; immediate ' exit , 0 state ! ;") >= 0);

/**
We now name all the registers so we can refer to them by name instead of by
number, this is not strictly necessary but is good practice.
**/
	for(i = 0; register_names[i]; i++)
		F_VERIFY(forth_define_constant(o, register_names[i], i+DIC) >= 0);

/**
More constants are now defined:
**/
	F_VERIFY(forth_define_constant(o, "size", sizeof(forth_cell_t)) >= 0);
	F_VERIFY(forth_define_constant(o, "stack-start", size - (2 * o->m[STACK_SIZE])) >= 0);
	F_VERIFY(forth_define_constant(o, "max-core", size) >= 0);
	F_VERIFY(forth_define_constant(o, "r/o",      FAM_RO) >= 0);
	F_VERIFY(forth_define_constant(o, "w/o",      FAM_WO) >= 0);
	F_VERIFY(forth_define_constant(o, "r/w",      FAM_RW) >= 0);
	F_VERIFY(forth_define_constant(o, "dictionary-start",  DICTIONARY_START) >= 0);
	F_VERIFY(forth_define_constant(o, "tib",  STRING_OFFSET * sizeof(forth_cell_t)) >= 0);
	F_VERIFY(forth_define_constant(o, "#tib", MAXIMUM_WORD_LENGTH * sizeof(forth_cell_t)) >= 0);

/**
Now we finally are in a state to load the slightly inaccurately
named **initial_forth_program**, which will give us basic looping and
conditional constructs 
**/
	F_VERIFY(forth_eval(o, initial_forth_program) >= 0);


/**All of the calls to **forth_eval** and **forth_define_constant** have
set the input streams to point to a string, we need to reset them
to they point to the file **in**
**/
	forth_set_file_input(o, in);  /*set up input after our eval */
	return o;
}

/**
This is a crude method that should only be used for debugging purposes, it
simply dumps the forth structure to disk, including any padding which the
compiler might have inserted. This dump cannot be reloaded 
**/
int forth_dump_core(forth_t *o, FILE *dump)
{
	assert(o && dump);
	size_t w = sizeof(*o) + sizeof(forth_cell_t) * o->core_size;
	return w != fwrite(o, 1, w, dump) ? -1: 0;
}

/** 
We can save the virtual machines working memory in a way, called serialization,
such that we can load the saved file back in and continue execution using this
save environment. Only the three previously mentioned fields are serialized;
**m**, **core_size** and the **header**.
**/
int forth_save_core_file(forth_t *o, FILE *dump)
{
	assert(o && dump);
	uint64_t r1, r2, core_size = o->core_size;
	if(forth_is_invalid(o))
		return -1;
	r1 = fwrite(o->header,  1, sizeof(o->header), dump);
	r2 = fwrite(o->m,       1, sizeof(forth_cell_t) * core_size, dump);
	if(r1+r2 != (sizeof(o->header) + sizeof(forth_cell_t) * core_size))
		return -1;
	return 0;
}

/** 
Logically if we can save the core for future reuse, then we must have a
function for loading the core back in, this function returns a reinitialized
Forth object. Validation on the object is performed to make sure that it is
a valid object and not some other random file, endianess, **core_size**, cell
size and the headers magic constants field are all checked to make sure they
are correct and compatible with this interpreter.

**forth_make_default** is called to replace any instances of pointers stored
in registers which are now invalid after we have loaded the file from disk.
**/
forth_t *forth_load_core_file(FILE *dump)
{ 
	uint8_t actual[sizeof(header)] = {0},   /* read in header */
		expected[sizeof(header)] = {0}; /* what we expected */
	forth_t *o = NULL;
	uint64_t w = 0, core_size = 0;
	assert(dump);
	make_header(expected, 0);
	if(sizeof(actual) != fread(actual, 1, sizeof(actual), dump)) {
		goto fail; /* no header */
	}
	if(memcmp(expected, actual, sizeof(header)-1)) {
		goto fail; /* invalid or incompatible header */
	}
	core_size = 1 << actual[LOG2_SIZE];

	if(core_size < MINIMUM_CORE_SIZE) {
		error("core size of %"PRIdCell" is too small", core_size);
		goto fail; 
	}
	w = sizeof(*o) + (sizeof(forth_cell_t) * core_size);
	errno = 0;
	if(!(o = calloc(w, 1))) {
		error("allocation of size %"PRId64" failed, %s", w, forth_strerror());
		goto fail; 
	}
	w = sizeof(forth_cell_t) * core_size;
	if(w != fread(o->m, 1, w, dump)) {
		error("file too small (expected %"PRId64")", w);
		goto fail;
	}
	o->core_size = core_size;
	memcpy(o->header, actual, sizeof(o->header));
	forth_make_default(o, core_size, stdin, stdout);
	return o;
fail:
	free(o);
	return NULL;
}

/**
The following function allows us to load a core file from memory:
**/
forth_t *forth_load_core_memory(char *m, size_t size)
{
	/**@warning As the version of libforth is intended for an embedded
	 * environment, how objects are allocated will have to change. */
	assert(m && (size / sizeof(forth_cell_t)) >= MINIMUM_CORE_SIZE);
	forth_t *o;
	size_t offset = sizeof(o->header);
	size -= offset;
	errno = 0;
	o = calloc(sizeof(*o), 1); 
	if(!o) {
		error("allocation of size %zu failed, %s", sizeof(*o) + size, forth_strerror());
		return NULL;
	}
	make_header(o->header, forth_blog2(size));
	//memcpy(o->m, m + offset, size);
	o->m = (forth_cell_t*)(m + offset);
	forth_make_default(o, size / sizeof(forth_cell_t), stdin, stdout);
	return o;
}

/**
And likewise we will want to be able to save to memory as well, the
load and save functions for memory expect headers *not* to be present.
**/
char *forth_save_core_memory(forth_t *o, size_t *size)
{
	assert(o && size);
	char *m;
	*size = 0;
	errno = 0;
	uint64_t w = o->core_size;
	m = malloc(w * sizeof(forth_cell_t) + sizeof(o->header));
	if(!m) {
		error("allocation of size %zu failed, %s", 
				o->core_size * sizeof(forth_cell_t), forth_strerror());
		return NULL;
	}
	memcpy(m, o->header, sizeof(o->header)); /* copy header */
	memcpy(m + sizeof(o->header), o->m, w); /* core */
	*size = o->core_size * sizeof(forth_cell_t) + sizeof(o->header);
	return m;
}

/**
Free the Forth interpreter, we make sure to invalidate the interpreter
in case there is a use after free.
**/
void forth_free(forth_t *o)
{
	assert(o);
	/* invalidate the forth core, a sufficiently "smart" compiler 
	 * might optimize this out */
	//forth_invalidate(o); @todo fix allocation for embedded version
	free(o);
}

/**
Unfortunately C disallows the static initialization of structures with 
flexible array member, GCC allows this as an extension.
**/
struct forth_functions *forth_new_function_list(forth_cell_t count)
{
	struct forth_functions *ff = NULL;
	errno = 0;
	ff = calloc(sizeof(*ff), 1);
	ff->functions = calloc(sizeof(ff->functions[0]) * count, 1);
	if(!ff || !ff->functions) 
		warning("calloc failed: %s", forth_strerror());
	else
		ff->count = count;
	return ff;
}

void forth_delete_function_list(struct forth_functions *calls)
{
	assert(calls);
	free(calls->functions);
	free(calls);
}

/**
**forth_push**, **forth_pop** and **forth_stack_position** are the main 
ways an application programmer can interact with the Forth interpreter. Usually
this tutorial talks about how the interpreter and virtual machine work,
about how compilation and command modes work, and the internals of a Forth
implementation. However this project does not just present an ordinary Forth
interpreter, the interpreter can be embedded into other applications, and it is
possible be running multiple instances Forth interpreters in the same process.

The project provides an API which other programmers can use to do this, one
mechanism that needs to be provided is the ability to move data into and
out of the interpreter, these C level functions are how this mechanism is
achieved. They move data between a C program and a paused Forth interpreters
variable stack.
**/

void forth_push(forth_t *o, forth_cell_t f)
{
	assert(o && o->S < o->m + o->core_size);
	*++(o->S) = o->m[TOP];
	o->m[TOP] = f;
}

forth_cell_t forth_pop(forth_t *o)
{
	assert(o && o->S > o->m);
	forth_cell_t f = o->m[TOP];
	o->m[TOP] = *(o->S)--;
	return f;
}

forth_cell_t forth_stack_position(forth_t *o)
{
	assert(o);
	return o->S - o->vstart;
}

/**
## The Forth Virtual Machine
**/

/**
The largest function in the file, which implements the forth virtual
machine, everything else in this file is just fluff and support for this
function. This is the Forth virtual machine, it implements a threaded
code interpreter (see <https://en.wikipedia.org/wiki/Threaded_code>, and
<https://www.complang.tuwien.ac.at/forth/threaded-code.html>).
**/
int forth_run(forth_t *o)
{
	int errorval = 0;
	assert(o);
	jmp_buf on_error;
	if(forth_is_invalid(o)) {
		fatal("refusing to run an invalid forth, %"PRIdCell, forth_is_invalid(o));
		return -1;
	}

	/* The following code handles errors, if an error occurs, the
	 * interpreter will jump back to here.
	 *
	 * @todo This code needs to be rethought to be made more compliant with
	 * how "throw" and "catch" work in Forth. */
	if ((errorval = setjmp(on_error)) || forth_is_invalid(o)) {
		/* if the interpreter is invalid we always exit*/
		if(forth_is_invalid(o))
			return -1;
		switch(errorval) {
			default:
			case FATAL:
				forth_invalidate(o);
				return -1;
			/* recoverable errors depend on o->m[ERROR_HANDLER],
			 * a register which can be set within the running
			 * virtual machine. */
			case RECOVERABLE:
				switch(o->m[ERROR_HANDLER]) {
				case ERROR_INVALIDATE: 
					forth_invalidate(o);
				case ERROR_HALT:       
					return -forth_is_invalid(o);
				case ERROR_RECOVER:    
					o->m[RSTK] = o->core_size - o->m[STACK_SIZE];
					break;
				}
			case OKAY: 
				break;
		}
	}
	
	forth_cell_t *m = o->m,  /* convenience variable: virtual memory */
		     pc,         /* virtual machines program counter */
		     *S = o->S,  /* convenience variable: stack pointer */
		     I = o->m[INSTRUCTION], /* instruction pointer */
		     f = o->m[TOP], /* top of stack */
		     w,          /* working pointer */
		     clk;        /* clock variable */

	clk = 1000; //(1000 * clock()) / CLOCKS_PER_SEC;

/**
The following section will explain how the threaded virtual machine interpreter
works. Threaded code is a simple concept and Forths typically compile
their code to threaded code, it suites Forth implementations as word
definitions consist of juxtaposition of previously defined words until they
reach a set of primitives.

This means a function like **square** will be implemented like this:

	call dup   <- duplicate the top item on the variable stack
	call *     <- push the result of multiplying the top two items
	call exit  <- exit the definition of square

Each word definition is like this, a series of calls to other functions. We
can optimize this by removing the explicit **call** and just having a series
of code address to jump to, which will become:

	address of "dup"
	address of "*"
	address of "exit"

We now have the problem that we cannot just jump to the beginning of the
definition of **square** in our virtual machine, we instead use an instruction
(**RUN** in our interpreter, or **DOLIST** as it is sometimes known in most 
other implementations) to determine what to do with the following data, if there
is any. This system also allows us to encode primitives, or virtual machine
instructions, in the same way as we encode words. If our word does not have
the **RUN** instruction as its first instruction then the list of addresses will
not be interpreted but only a simple instruction will be executed.

The for loop and the switch statement here form the basis of our thread code
interpreter along with the program counter register (**pc**) and the instruction
pointer register (**I**).

To explain how execution proceeds it will help to refer to the internal
structure of a word and how words are compiled into the dictionary.

Above we saw that a words layout looked like this:

	.-----------.-----.------.--------.------------.
	| Word Name | PWD | MISC | CODE-2 | Data Field |
	.-----------.-----.------.--------.------------.

During execution we do not care about the **Word Name** field and **PWD**
field. Also during execution we do not care about the top bits of the **MISC**
field, only what instruction it contains.

Immediate words looks like this:

	.-------------.---------------------.
	| Instruction | Optional Data Field |
	.-------------.---------------------.

And compiling words look like this:

	.---------.-------------.---------------------.
	| COMPILE | Instruction | Optional Data Field |
	.---------.-------------.---------------------.

If the data field exists, the **Instruction** field will contain **RUN**. 
For words that only implement a single virtual machine instruction the 
**Instruction** field will contain only that single instruction 
(such as ADD, or SUB).

Let us define a series of words and see how the resulting word definitions
are laid out, discounting the **Word Name**, **PWD** and the top bits of the
**MISC** field.

We will define two words **square** (which takes a number off the stack,
multiplies it by itself and pushes the result onto the stack) and
**sum-of-products** (which takes two numbers off the stack, squares each one,
adds the two results together and pushes the result onto the stack):

	: square           dup * ;
	: sum-of-products  square swap square + ;

Executing these:

	9 square .            => prints '81 '
	3 4 sum-of-products . => prints '25 '

1) **square** refers to two built in words **dup** and **\***,
2) **sum-of-products** to the word we just defined and two built in words
3) **swap** and **+**. We have also used the immediate word **:** and **;**.

Definition of **dup**, a compiling word:

	.---------.------.
	| COMPILE | DUP  |
	.---------.------.

Definition of **+**, a compiling word:

	.---------.------.
	| COMPILE | +    |
	.---------.------.

Definition of **swap**, a compiling word:

	.---------.------.
	| COMPILE | SWAP |
	.---------.------.

Definition of **exit**, a compiling word:

	.---------.------.
	| COMPILE | EXIT |
	.---------.------.

Definition of **:**, an immediate word:

	.---.
	| : |
	.---.

Definition of **;**, a defined immediate word:

	.-----.----.-------.----.-----------.--------.-------.
	| RUN | $' | $exit | $, | literal 0 | $state | $exit |
	.-----.----.-------.----.-----------.--------.-------.

Definition of **square**, a defined compiling word:

	.---------.-----.------.----.-------.
	| COMPILE | RUN | $dup | $* | $exit |
	.---------.-----.------.----.-------.

Definition of **sum-of-products**, a defined compiling word:

	.---------.-----.---------.-------.---------.----.-------.
	| COMPILE | RUN | $square | $swap | $square | $+ | $exit |
	.---------.-----.---------.-------.---------.----.-------.

All of these words are defined in the dictionary, which is a
separate data structure from the variable stack. In the above
definitions we use **$square** or **$\*** to mean a pointer to the words
run time behavior, this is never the **COMPILE** field. **literal 0**
means that at run time the number 0 is pushed to the variable stack,
also the definition of **state** is not shown, as that would
complicate things.

Imagine we have just typed in "sum-of-products" with "3 4" on the
variable stack. Our **pc** register is now pointing the **RUN** field of
sum of products, the virtual machine will next execute the **RUN**
instruction, saving the instruction pointer to the return stack for
when we finally exit **sum-of-products** back to the interpreter.
**square** will now be called, it's **RUN** field encountered, then **dup**.
**dup** does not have a **RUN** field, it is a built in primitive, so the
instruction pointer will not be touched nor the return stack, but the
**DUP** instruction will now be executed. 

After this has run the instruction pointer will now be moved to executed
**\***, another primitive, then **exit** - which pops a value off the return
stack and sets the instruction pointer to that value. The value points to
the **$swap** field in **sum-of-products**, which will in turn be executed
until the final **$exit** field is encountered. This exits back into our
special read-and-loop word defined in the initialization code.

The **READ** routine must make sure the correct field is executed when
a word is read in which depends on the state of the interpreter (held
in **STATE** register). 

It should be noted that for compatibility with future versions of the
virtual machine that instructions can be added to the end (after the last 
defined instruction) but not removed.
**/
	for(;(pc = m[ck(I++)]);) { 
	INNER:  
		w = instruction(m[ck(pc++)]);
		TRACE(o, w, S, f);
		switch (w) { 

/**
When explaining words with example Forth code the
instructions enumeration will not be used (such as **ADD** or
**SUB**), but its name will be used instead (such as **+** or **-**) 
**/

		case PUSH:    *++S = f;     f = m[ck(I++)];          break;
		case COMPILE: m[dic(m[DIC]++)] = pc;                 break; 
		case RUN:     m[ck(++m[RSTK])] = I; I = pc;          break;
		case DEFINE:
/**
**DEFINE** backs the Forth word **:**, which is an immediate word, it reads in a
new word name, creates a header for that word and enters into compile mode,
where all words (baring immediate words) are compiled into the dictionary
instead of being executed.

The created header looks like this:

	 .------.-----.------.-----.----
	 | NAME | PWD | MISC | RUN |    ...
	 .------.-----.------.-----.----
				      ^
				      |
				    Dictionary Pointer 
**/

			m[STATE] = 1; /* compile mode */
			if(forth_get_word(o, o->s) < 0)
				goto end;
			compile(o, COMPILE, (char*)o->s);
			m[dic(m[DIC]++)] = RUN;
			break;
		case IMMEDIATE:
/**
**IMMEDIATE** makes the current word definition execute regardless of whether we
are in compile or command mode. Unlike most Forths this needs to go right after
the word to be defined name instead of after the word definition itself. I
prefer this behavior, however the reason for this is due to implementation
reasons and not because of this preference.

So our interpreter defines immediate words:

	: name immediate ... ;

versus, as is expected:

	: name ... ; immediate

The way this word works is when **DEFINE** (or **:**) runs it creates a word 
header that looks like this:

	.------.-----.------.-----.----
	| NAME | PWD | MISC | RUN |    ...
	.------.-----.------.-----.----
				     ^
				     |
				   Dictionary Pointer


Where the **MISC** field contains **COMPILE**, we want it to look like this:

	.------.-----.------.----
	| NAME | PWD | MISC |    ...
	.------.-----.------.----
			       ^
			       |
			       Dictionary Pointer

With the **MISC** field containing **RUN**. 
**/
			m[DIC] -= 2; /* move to first code field */
			m[m[DIC]] &= ~INSTRUCTION_MASK; /* zero instruction */
			m[m[DIC]] |= RUN; /* set instruction to RUN */
			dic(m[DIC]++); /* compilation start here */ 
			break;
		case READ:
/**
The **READ** instruction, an instruction that usually does not belong in a
virtual machine, forms the basis of Forths interactive nature. In order to 
move this word outside of the virtual machine a compiler for the virtual
machine would have to be made, which would complicate the implementation,
but simplify the virtual machine and make it more like a 'normal' virtual
machine.

It attempts to do the follow:

a) Lookup a space delimited string in the Forth dictionary, if it is found
and we are in command mode we execute it, if we are in compile mode and
the word is a compiling word we compile a pointer to it in the dictionary,
if not we execute it.
b) If it is not a word in the dictionary we attempt to treat it as a number,
if it is numeric (using the **BASE** register to determine the base)
then if we are in command mode we push the number to the variable stack,
else if we are in compile mode we compile the literal into the dictionary.
c) If it is neither a word nor a number, regardless of mode, we emit a
diagnostic.

This is the most complex word in the Forth virtual machine, there is a good
case for it being moved outside of it, and perhaps this will happen. You
will notice that the above description did not include any looping, as such
there is a driver for the interpreter which must be made and initialized
in **forth_init**, a simple word that calls **READ** in a loop (actually tail
recursively).

**/
			if(forth_get_word(o, o->s) < 0)
				goto end;
			if ((w = forth_find(o, (char*)o->s)) > 1) {
				pc = w;
				if (!m[STATE] && instruction(m[ck(pc)]) == COMPILE)
					pc++; /* in command mode, execute word */
				goto INNER;
			} else if(forth_string_to_cell(o->m[BASE], &w, (char*)o->s)) {
				error("'%s' is not a word", o->s);
				longjmp(on_error, RECOVERABLE);
				break;
			}
			if (m[STATE]) { /* must be a number then */
				m[dic(m[DIC]++)] = 2; /*fake word push at m[2] */
				m[dic(m[DIC]++)] = w;
			} else { /* push word */
				*++S = f;
				f = w;
			}
			break;
/**
Most of the following Forth instructions are simple Forth words, each one
with an uncomplicated Forth word which is implemented by the corresponding
instruction (such as LOAD and "@", STORE and "!", EXIT and "exit", and ADD
and "+").

However, the reason for these words existing, and under what circumstances
some of the can be used is a different matter, the COMMA and TAIL word will
require some explaining, but ADD, SUB and DIV will not.
**/
		case LOAD:    cd(1); f = m[ck(f)];                   break;
		case STORE:   cd(2); m[ck(f)] = *S--; f = *S--;      break;
		case CLOAD:   cd(1); f = *(((uint8_t*)m) + ckchar(f)); break;
		case CSTORE:  cd(2); ((uint8_t*)m)[ckchar(f)] = *S--; f = *S--; break;
		case SUB:     cd(2); f = *S-- - f;                   break;
		case ADD:     cd(2); f = *S-- + f;                   break;
		case AND:     cd(2); f = *S-- & f;                   break;
		case OR:      cd(2); f = *S-- | f;                   break;
		case XOR:     cd(2); f = *S-- ^ f;                   break;
		case INV:     cd(1); f = ~f;                         break;
		case SHL:     cd(2); f = *S-- << f;                  break;
		case SHR:     cd(2); f = *S-- >> f;                  break;
		case MUL:     cd(2); f = *S-- * f;                   break;
		case DIV:
			cd(2);
			if(f) {
				f = *S-- / f;
			} else {
				error("divide %"PRIdCell" by zero ", *S--);
				longjmp(on_error, RECOVERABLE);
			} 
			break;
		case ULESS:   cd(2); f = *S-- < f;                       break;
		case UMORE:   cd(2); f = *S-- > f;                       break;
		case EXIT:    I = m[ck(m[RSTK]--)];                      break;
		case KEY:     *++S = f; f = forth_get_char(o);           break;
		case EMIT:    f = fputc(f, (FILE*)o->m[FOUT]);           break;
		case FROMR:   *++S = f; f = m[ck(m[RSTK]--)];            break;
		case TOR:     cd(1); m[ck(++m[RSTK])] = f; f = *S--;     break;
		case BRANCH:  I += m[ck(I)];                             break;
		case QBRANCH: cd(1); I += f == 0 ? m[I] : 1; f = *S--;   break;
		case PNUM:    cd(1); 
			      f = print_cell(o, (FILE*)(o->m[FOUT]), f); break;
		case QUOTE:   *++S = f;     f = m[ck(I++)];              break;
		case COMMA:   cd(1); m[dic(m[DIC]++)] = f; f = *S--;     break;
		case EQUAL:   cd(2); f = *S-- == f;                      break;
		case SWAP:    cd(2); w = f;  f = *S--;   *++S = w;       break;
		case DUP:     cd(1); *++S = f;                           break;
		case DROP:    cd(1); f = *S--;                           break;
		case OVER:    cd(2); w = *S; *++S = f; f = w;            break;
/**
**TAIL** is a crude method of doing tail recursion, it should not be used 
generally but is useful at startup, there are limitations when using it 
in word definitions.

The following tail recursive definition of the greatest common divisor,
called **(gcd)** will not work correctly when interacting with other words:

	: (gcd) ?dup if dup rot rot mod tail (gcd) then ;

If we define a word:

	: uses-gcd 50 20 (gcd) . ;

We might expect it to print out "10", however it will not, it will calculate
the GCD, but not print it out with ".", as GCD will have popped off where
it should have returned.

Instead we must wrap the definition up in another definition:

	: gcd (gcd) ;

And the definition **gcd** can be used. There is a definition of **tail** within
*forth.fth* that does not have this limitation, in fact the built in definition
is hidden in favor of the new one.
**/
		case TAIL:
			m[RSTK]--;
			break;
/** 
FIND is a natural factor of READ, we add it to the Forth interpreter as
it already exits, it looks up a Forth word in the dictionary and returns a
pointer to that word if it found.
**/
		case FIND:
			*++S = f;
			if(forth_get_word(o, o->s) < 0)
				goto end;
			f = forth_find(o, (char*)o->s);
			f = f < DICTIONARY_START ? 0 : f;
			break;

/**
DEPTH is added because the stack is not directly accessible
by the virtual machine, normally it would have no way of knowing 
where the variable stack pointer is, which is needed to implement 
Forth words such as **.s** - which prints out all the
items on the stack.
**/
		case DEPTH:
			w = S - o->vstart;
			*++S = f;
			f = w;
			break;
/**
SPLOAD (**sp@**) loads the current stack pointer, which is needed because the
stack pointer does not live within any of the virtual machines registers.
**/
		case SPLOAD:
			*++S = f;
			f = (forth_cell_t)(S - o->m);
			break;
/**
SPSTORE (**sp!**) modifies the stack, setting it to the value on the top
of the stack.
**/
		case SPSTORE:
			w = *S--;
			S = (forth_cell_t*)(f + o->m - 1);
			f = w;
			break;
/**
EVALUATOR is another complex word which needs to be implemented in
the virtual machine. It saves and restores state which we do
not usually need to do when the interpreter is not running (the usual case
for **forth_eval** when called from C). It can read either from a string
or from a file.
**/
		case EVALUATOR:
		{ 
			/* save current input */
			forth_cell_t sin    = o->m[SIN],  sidx = o->m[SIDX],
				slen   = o->m[SLEN], fin  = o->m[FIN],
				source = o->m[SOURCE_ID], r = m[RSTK];
			char *s = NULL;
			FILE *file = NULL;
			int file_in = 0;
			cd(3);
			file_in = f; /*get file/string in bool*/
			f = *S--;
			if(file_in) {
				file = (FILE*)(*S--);
				f = *S--;
			} else {
				s = forth_get_string(o, &on_error, &S, f);
				f = *S--;
			}
			/* save the stack variables */
			o->S = S;
			o->m[TOP] = f;
			/* push a fake call to forth_eval */
			m[RSTK]++;
			if(file_in) {
				forth_set_file_input(o, file);
				w = forth_run(o);
			} else {
				w = forth_eval(o, s);
			}
			/* restore stack variables */
			m[RSTK] = r;
			S = o->S;
			*++S = o->m[TOP];
			f = w;
			/* restore input stream */
			o->m[SIN]  = sin;
			o->m[SIDX] = sidx;
			o->m[SLEN] = slen;
			o->m[FIN]  = fin;
			o->m[SOURCE_ID] = source;
			if(forth_is_invalid(o))
				return -1;
		}
		break;
		case PSTK:    print_stack(o, (FILE*)(o->m[STDOUT]), S, f);   break;
		case RESTART: cd(1); longjmp(on_error, f);                   break;

/**
CALL allows arbitrary C functions to be passed in and used within
the interpreter, allowing it to be extended. The functions have to be
passed in during initialization and then they become available to be
used by CALL.

The structure **forth_functions** is a list of function
pointers that can be populated by the user of the libforth library,
CALL indexes into that structure (after performing bounds checking)
and executes the function.
**/
		case CALL:
		{
			cd(1);
			if(!(o->calls) || !(o->calls->count)) {
				/* no call structure, or count is zero */
				f = -1;
				break;
			}
			forth_cell_t i = f;
			if(i >= (o->calls->count)) {
				f = -1;
				break;
			}

			assert(o->calls->functions[i].function);
			/* check depth of function */
			cd(o->calls->functions[i].depth);
			/* pop call number */
			f = *S--; 
			/* save stack state */
			o->S = S;
			o->m[TOP] = f;
			/* call arbitrary C function */
			w = o->calls->functions[i].function(o);
			/* restore stack state */
			S = o->S;
			f = o->m[TOP];
			/* push call success value */
			*++S = f;
			f = w;
			break;
		}
		case CLOCK: 
			/**@todo NuttX: implement something sensible here */
			fprintf(stderr, "clock: not implemented\n");
			break;
		case SYSTEM: cd(2); 
			     *S--;
			     f = -21; /* unsupported operation */
			     break;
/**
Whilst loathe to put these in here as virtual machine instructions (instead
a better mechanism should be found), this is the simplest way of adding file
access words to our Forth interpreter.

The file access methods *should* all be wrapped up so it does not matter
if a file or a piece of memory (a string for example) is being read or
written to. This would allow the KEY to be removed as a virtual machine
instruction, and would be a useful abstraction. 
**/

		case FCLOSE:  cd(1); 
			      errno = 0;
			      f = fclose((FILE*)f) ? errno : 0;       
			      break;
		case FDELETE: cd(2); 
			      errno = 0;
			      f = remove(forth_get_string(o, &on_error, &S, f)) ? errno : 0; 
			      break;
		case FFLUSH:  cd(1); 
			      errno = 0; 
			      f = fflush((FILE*)f) ? errno : 0;       
			      break;
		case FSEEK:   
			{
				cd(2); 
				errno = 0;
				int r = fseek((FILE*)(*S--), f, SEEK_SET);
				f = r == -1 ? errno || -1 : 0;
				break;
			}
		case FPOS:    
			{
				cd(1); 
				errno = 0;
				int r = ftell((FILE*)f);
				*++S = r;
				f = r == -1 ? errno || -1 : 0;
				break;
			}
		case FOPEN: 
			cd(3);
			{
				const char *fam = forth_get_fam(&on_error, f);
				f = *S--;
				char *file = forth_get_string(o, &on_error, &S, f);
				errno = 0;
				*++S = (forth_cell_t)fopen(file, fam);
				f = errno;
			}
			break;
		case FREAD:
			cd(3);
			{
				FILE *file = (FILE*)f;
				forth_cell_t count = *S--;
				forth_cell_t offset = *S--;
				*++S = fread(((char*)m)+offset, 1, count, file);
				f = ferror(file);
				clearerr(file);
			}
			break;
		case FWRITE:
			cd(3);
			{
				FILE *file = (FILE*)f;
				forth_cell_t count = *S--;
				forth_cell_t offset = *S--;
				*++S = fwrite(((char*)m)+offset, 1, count, file);
				f = ferror(file);
				clearerr(file);
			}
			break;
		case FRENAME:  
			cd(3); 
			{
				const char *f1 = forth_get_fam(&on_error, f);
				f = *S--;
				char *f2 = forth_get_string(o, &on_error, &S, f);
				errno = 0;
				f = -21; /* unsupported operation */
				//f = rename(f2, f1) ? errno : 0;
			}
			break;
		case RAISE:
			f = raise(f);
			break;
		case DATE:
			{
				time_t raw;
				struct tm *gmt;
				time(&raw);
				gmt = gmtime(&raw);
				*++S = f;
				*++S = gmt->tm_sec;
				*++S = gmt->tm_min;
				*++S = gmt->tm_hour;
				*++S = gmt->tm_mday;
				*++S = gmt->tm_mon;
				*++S = gmt->tm_year + 1900;
				*++S = gmt->tm_wday;
				*++S = gmt->tm_yday;
				f    = gmt->tm_isdst;
				break;
			}
/**
The following memory functions can be used by the Forth interpreter
for faster memory operations, but more importantly they can be used
to interact with memory outside of the Forth core.
**/
		case MEMMOVE:
			w = *S--;
			memmove((char*)(*S--), (char*)w, f);
			f = *S--;
			break;
		case MEMCHR:
			w = *S--;
			f = (forth_cell_t)memchr((char*)(*S--), w, f);
			break;
		case MEMSET:
			w = *S--;
			memset((char*)(*S--), w, f);
			f = *S--;
			break;
		case MEMCMP:
			w = *S--;
			f = memcmp((char*)(*S--), (char*)w, f);
			break;
		case ALLOCATE:
			errno = 0;
			*++S = (forth_cell_t)calloc(f, 1);
			f = errno;
			break;
		case FREE:
/**
It is not likely that the C library will set the errno if it detects a
problem, it will most likely either abort the program or silently
corrupt the heap if something goes wrong, however the Forth standard
requires that an error status is returned.
**/
			errno = 0;
			free((char*)f);
			f = errno;
			break;
		case RESIZE:
			errno = 0;
			w = (forth_cell_t)realloc((char*)(*S--), f);
			*++S = w;
			f = errno;
			break;

/**
This should never happen, and if it does it is an indication that virtual
machine memory has been corrupted somehow.
**/
		default:
			fatal("illegal operation %" PRIdCell, w);
			longjmp(on_error, FATAL);
		}
	}
/**
We must save the stack pointer and the top of stack when we exit the
interpreter so the C functions like "forth_pop" work correctly. If the
**forth_t** object has been invalidated (because something went wrong),
we do not have to jump to *end* as functions like **forth_pop** should not
be called on the invalidated object any longer.
**/
end:	o->S = S;
	o->m[TOP] = f;
	return 0;
}

/**    
## An example main function called **main_forth**

This is a very simple, limited, example of what can be done with the
libforth.

This make implementing a Forth interpreter as simple as:

	==== main.c =============================

	#include "libforth.h"

	int main(int argc, char **argv)
	{
	   return main_forth(argc, argv);
	}

	==== main.c =============================

**/

int main_forth(int argc, char **argv)
{
	FILE *core = fopen("forth.core", "rb");
	forth_t *o = NULL;
	int r = 0;
	if(core) {
		o = forth_load_core_file(core);
		fclose(core);
	}
	if(!o)
		o = forth_init(DEFAULT_CORE_SIZE, stdin, stdout, NULL);
	if(!o) {
		fatal("failed to initialize forth: %s", forth_strerror());
		return -1;
	}
	forth_set_args(o, argc, argv);
	if((r = forth_run(o)) < 0)
		return r;
	errno = 0;
	if(!(core = fopen("forth.core", "wb"))) {
		fatal("failed to save core file: %s", forth_strerror());
		return -1;
	}
	fclose(core);
	r = forth_save_core_file(o, core);
	forth_free(o);
	return r;
}

/**
And that completes the program, and the documentation describing it.
**/
