import subprocess
import argparse
import os
import re

def render_name(command, help_):
    res = ""
    res += "% " + args.command.upper() + "(1)\n\n"
    short_description = help_.split("\n")[1].split("]")[1]
    res += "# NAME\n\n" + args.command + short_description + "\n\n"
    return res

def render_option_synopsis(option):
    res = "["
    option = option.split ()
    if not option:
        return ""
    res += option[0].split(",")[0]
    if option[1].startswith("--"):
        if "=" in option[1]:
            res += " " + option[1].split("=")[1]
        res += "|" + option[1]
    res += "]\n"
    return res

def render_synopsis(help_, command, sections):
    res = "# SYNOPSIS\n\n"
    res += "```\n"
    res += command + " "
    split = help_.split("\n")
    for section in sections:
        add_option = False
        for l in split:
            if add_option == True:
                res += render_option_synopsis (l)
            if l == section:
                add_option = True
            elif not l.strip().startswith("-"):
                add_option = False
    res += "```\n\n"
    return res

def render_description(help_):
    res = "# DESCRIPTION\n\n"
    # Skip usage and stop at options
    for line in help_.split("\n")[2:]:
        if "Options" in line:
            break
        res += line + "\n"

    res += "\n\n"
    return res

def render_option(option):
    res = ""
    split = option.split()
    if not split:
        return res

    if split[0].startswith ("-") and split[1].startswith("--"):
        short_form = split[0]
        long_form = split[1]
        description = " ".join(split[2:])
    else:
        short_form = ""
        long_form = split[0]
        description = " ".join(split[1:])

    if ("=<" in long_form):
        arg = long_form.split("=<")[1]
        arg = re.sub("[<>]", "", arg)
        long_form = long_form.split("=<")[0]
    else:
        arg = ""

    short_form = short_form.split(",")[0]
    if short_form:
        res += short_form
        if arg:
            res += " *" + arg + "*"
        res += ", "

    res += "\\" + long_form
    if arg:
        res += "=*" + arg + "*"
    res += "\n:    " + description + "\n\n"
    return res

def render_options(help_, sections):
    res = "# OPTIONS\n\n"
    split = help_.split("\n")
    for section in sections:
        res += "\n\n## " + section + "\n\n"
        add_option = False
        for l in split:
            if add_option == True:
                res += render_option (l)
            if l == section:
                add_option = True
            elif not l.strip().startswith("-"):
                add_option = False
    return res

if __name__=="__main__":
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-c", "--command",
                            action="store", dest="command",
                            default="",
                            help="Command to parse help from")
    arg_parser.add_argument("-o", "--output",
                            action="store", dest="output",
                            default="",
                            help="Directory to write manual to")
    arg_parser.add_argument("extra_sections", nargs='*',
                            help="The extra sections to document")
    args = arg_parser.parse_args()
    arg_parser = argparse.ArgumentParser()
    if not args.command:
        print ("please specify a command to parse help from")
        exit (1)
    if not args.output:
        args.output = os.getcwd()

    print (args.extra_sections)
    with open (os.path.join (args.output, args.command + ".markdown"), "w") as f:
        help_ = subprocess.check_output([args.command, "--help-all"],
                universal_newlines=True)

        output = ""
        output += render_name (args.command, help_)
        args.extra_sections.insert(0, "Application Options:")
        output += render_synopsis (help_, args.command, args.extra_sections)
        output += render_description(help_)
        output += render_options(help_, args.extra_sections)
        f.write(output)
