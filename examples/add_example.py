#!/usr/bin/env python

import argparse
from subprocess import call

try:
    from pycparser import c_parser, c_ast

    def translate_C_prototype_to_python (name, prototype=None):
        result = "def "
        if prototype:
            c_decl = prototype + ";"
        else:
            return "def " + name + "()\n"

        parser = c_parser.CParser()
        node = parser.parse(c_decl, filename='<stdin>')
        func = node.ext[0]
        result += func.name + "("
        l = len (func.type.args.params) - 1
        for i, arg in enumerate(func.type.args.params):
            result += arg.name
            if i != l:
                result += ", "
        result += "):\n"
        return result

except ImportError:
    print """
        WARNING: no pycparser module, prototype translation disabled.
        Install with "pip install pycparser"
    """
    def translate_C_prototype_to_python (name, prototype=None):
        return "def " + name + "()\n"

def update_makefile_for_C_executable (name):
    sed_arg = r's/.*noinst_PROGRAMS =.*/&\n        ' + name + r' \\/'
    call (["sed", "-i", sed_arg, "./c/Makefile.am"])
    call (["git", "add", "./c/Makefile.am"])
    print "Modified and git added ./c/Makefile.am"

def update_makefile_for_C_function (name):
    sed_arg = r's/.*libexamples_la_SOURCES=.*/&\n           ' + name + ".c" + r' \\/'
    call (["sed", "-i", sed_arg, "./c/Makefile.am"])
    call (["git", "add", "./c/Makefile.am"])
    print "Modified and git added ./c/Makefile.am"

def update_makefile_for_python_executable (name):
    sed_arg = r's/.*EXTRA_DIST=.*/&\n           ' + name + ".py" + r' \\/'
    call (["sed", "-i", sed_arg, "./python/Makefile.am"])
    call (["git", "add", "./python/Makefile.am"])
    print "Modified and git added ./python/Makefile.am"

def update_makefile_for_python_function (name):
    sed_arg = r's/.*EXTRA_DIST=.*/&\n           ' + name + ".py" + r' \\/'
    call (["sed", "-i", sed_arg, "./python/Makefile.am"])
    call (["git", "add", "./python/Makefile.am"])
    print "Modified and git added ./python/Makefile.am"

def update_makefile_for_shell_executable (name):
    sed_arg = r's/.*EXTRA_DIST=.*/&\n           ' + name + ".sh" + r' \\/'
    call (["sed", "-i", sed_arg, "./shell/Makefile.am"])
    call (["git", "add", "./shell/Makefile.am"])
    print "Modified and git added ./shell/Makefile.am"

def update_gitignore_for_C_executable (name):
    with open ("./c/.gitignore", "ab") as f:
        f.write(name + "\n")
    call (["git", "add", "./c/.gitignore"])
    print "Modified and git added ./c/.gitignore"

def update_header_for_C_function (name, prototype):
    if prototype:
        line = prototype + ";"
    else:
        line = "void " + name + "(void);"
    sed_arg = r's/.*#endif.*/' + line + r'\n&/'
    call (["sed", "-i", sed_arg, "./c/examples.h"])
 
    call (["git", "add", "./c/examples.h"])
    print "Modified and git added ./c/examples.h"

def create_file_for_C_function (name, prototype=None):
    filename = "./c/" + name + ".c"
    with open (filename, "wb") as f:
        f.write ('#include "examples.h"\n\n')
        if prototype:
            f.write (prototype + "\n{\n}\n")
        else:
            f.write ("void " + name + "(void)\n{\n}\n")

    call (["git", "add", filename])
    print "Created and git added", filename

def create_file_for_python_function (name, prototype=None):
    filename = "./python/" + name + ".py"
    with open (filename, "wb") as f:
        f.write ("from gi.repository import Gst, GES\n\n")
        python_proto = translate_C_prototype_to_python (name, prototype)
        f.write (python_proto)

    call (["git", "add", filename])
    print "Created and git added", filename

def create_file_for_C_executable (name, prototype=None):
    filename = "./c/" + name + ".c"
    with open (filename, "wb") as f:
        f.write ('#include "examples.h"\n\n')
        f.write ("int main (int args, char **argv)\n{\n")
        f.write ("  gst_init (NULL, NULL);\n")
        f.write ("  ges_init ();\n")
        f.write ("  return 0;\n}\n")

    call (["git", "add", filename])
    print "Created and git added", filename

def create_file_for_python_executable (name, prototype=None):
    filename = "./python/" + name + ".py"
    with open (filename, "wb") as f:
        f.write ("#!/usr/bin/env python\n\n")
        f.write ("from gi.repository import Gst, GES\n\n")
        f.write ('if __name__=="__main__":\n')
        f.write ('    Gst.init([])\n')
        f.write ('    GES.init()\n')

    call (["chmod", "755", filename])
    call (["git", "add", filename])
    print "Created, made executable and git added", filename

def create_file_for_shell_executable (name, prototype=None):
    filename = "./shell/" + name + ".sh"
    with open (filename, "wb") as f:
        f.write ("#!/bin/sh\n\n")

    call (["chmod", "755", filename])
    call (["git", "add", filename])
    print "Created, made executable and git added", filename

def update_markdown (name, language):
    with open (name + ".markdown", "ab") as f:
        f.write ('|[<!-- language="' + language.lower() + '" -->\n')
        if language == 'C':
            f.write ('{{ examples/c/' + name + '.c }}\n')
        elif language == 'python':
            f.write ('{{ examples/python/' + name + '.py }}\n')
        elif language == 'shell':
            f.write ('{{ examples/shell/' + name + '.sh }}\n')
        f.write (']|\n')

if __name__=="__main__":
    arg_parser = argparse.ArgumentParser(description="""
    Create example files, update Makefiles and gitgnores, and do version control stuff
    so that one only has to edit the created examples. Support functions and
    executables.

    Examples:
        + Add a new function in python and C with a default prototype:
            ./add_example.py foo -t function -l C,python

        + Add a new function in python and C with a custom prototype:
            ./add_example.py foo -t function -l C,python -p "int foo (char *bar)"

        + Add an executable in C, python and shell:
            ./add_example.py foo -l C,python,shell
    """,
    formatter_class=argparse.RawDescriptionHelpFormatter)
    arg_parser.add_argument("name", help="The new example to create")
    arg_parser.add_argument("-t", "--type", action="store", dest="type",
            default="executable", help="Type of example to add \
            [function|executable]")
    arg_parser.add_argument("-p", "--prototype", action="store", dest="prototype",
            default="", help="The prototype of the function to add")
    arg_parser.add_argument("-l", "--languages", action="store",
            dest="languages", default="C", help="The languages to add the \
            example for [C|shell|python], comma-separated")

    args = arg_parser.parse_args()

    for language in args.languages.split(","):
        try:
            func = globals()["update_makefile_for_" + language + "_" + args.type]
            func (args.name)
        except KeyError:
            print ("no update makefile function for ", language, args.type)

        try:
            func = globals()["create_file_for_" + language + "_" + args.type]
            func (args.name, args.prototype)
            update_markdown (args.name, language)
        except KeyError:
            print "no file creation function for ", language, args.type

        if language == "C":
            if args.type == "executable":
                update_gitignore_for_C_executable(args.name)
            else:
                update_header_for_C_function(args.name, args.prototype)

    filename = args.name + ".markdown"
    call (["git", "add", filename])
    print "Created and git added", filename
    call (["git", "status"])
