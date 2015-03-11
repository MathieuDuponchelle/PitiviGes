import xml.etree.ElementTree as ET
from xml.etree.ElementTree import QName
import argparse
import os
import errno

mime_map={"text/x-csrc": "c",
        "text/x-python": "python",
        "application/x-shellscript": "shell"}

def ensure_path(path):
    try:
        os.makedirs(path)
    except OSError as exception:
        if exception.errno != errno.EEXIST:
            raise

def namespace_tag(tag):
    new_tag = ""
    for string in tag.split("/"):
        new_tag += str(QName("http://projectmallard.org/1.0/", string)) + "/"
    new_tag = new_tag[:-1]
    return new_tag


def custom_findall(node, tag):
    new_tag = namespace_tag(tag)
    return node.findall(new_tag)


def custom_find(node, tag):
    new_tag = namespace_tag(tag)
    return node.find(new_tag)


def render_link(node):
    result = ""
    if "xref" in node.keys():
        xref = node.attrib["xref"]
        if node.text:
            text = node.text
        else:
            text = xref
        result = "[" + text + "](#" + xref.lower() + ")"
    elif "href" in node.keys():
        href = node.attrib["href"]
        if node.text:
            text = node.text
        else:
            text = href
        result = "[" + text + "](" + href + ")"

    if node.tail:
        result += node.tail.replace("#", "(FIXME broken link)")
    return result


def render_code_start(node, is_reference=False):
    mime = ""
    if "mime" in node.attrib:
        mime = node.attrib["mime"]
    if mime in mime_map:
        mime = mime_map[mime]
    else:
        mime = ""

    result = ""

    if not is_reference:
        if mime:
            result += "\n\n```" + mime
        else:
            result += '\n\n<pre class="inlined_code">'
    else:
        result += "**"
    return result

def render_code_end (node, is_reference=False):
    mime = ""
    result = ""

    if "mime" in node.attrib:
        mime = node.attrib["mime"]
    if mime in mime_map:
        mime = mime_map[mime]
    else:
        mime = ""
    if not is_reference:
        if mime:
            result += "\n```"
        else:
            result += "</pre>"
    else:
        result += "**"
    if node.tail:
        result += node.tail
    return result


def render_paragraph():
    return "\n\n"


def render_section (node, level):
    title = custom_find (node, "title").text
    return "\n\n###" + level * "#" + title + "\n"


def render_text(node):
    if node.text:
        return node.text.replace("#", "(FIXME broken link)")
    return ""


def render_tail(node):
    if node.tail:
        return node.tail.replace("#", "(FIXME broken link)")
    return ""


def render_prototype(params):
    result = "("
    for param in params:
        if param.name == "Returns":
            break
        if params[0] != param:
            result += ", "
        result += param.name
    result += ")"
    return result


def render_parameter_description(param):
    result = "*" + param.name + "*: "
    if param.description is not None:
        result += _parse_description(
            param.description, "", add_new_lines=False)
    else:
        result += "FIXME empty description"
    return result


def render_note(node):
    return "\n\n> "


def render_class(class_, output):
    output.write ("##" + class_.name + "\n")
    output.write (class_.description + "\n")


def render_function(function, output):
    output.write ("###" + function.name + "\n")
    output.write ("**" + function.prototype + "**\n\n")
    for description in function.parameter_descriptions:
        output.write (description + "\n")
    output.write (function.description + "\n")

# Reduced parsing, only look out for links.
def _parse_code (node):
    description = ""

    if node.text:
        description += node.text
    for n in node:
        ctag = n.tag.split('}')[1]
        if ctag == "link":
            description += render_link(n)
    return description

def _parse_description(node, description, add_new_lines=True, sections_level=0):
    tag = node.tag.split('}')[1]
    if tag not in ["page", "p", "section", "note"]:
        return description

    if tag == "section":
        sections_level += 1
        description += render_section (node, sections_level)

    if node.text:
        if add_new_lines:
            description += render_paragraph()

    description += render_text(node)

    for n in node:
        old_add_new_lines = add_new_lines
        ctag = n.tag.split('}')[1]
        if ctag == "link":
            description += render_link(n)
        elif ctag == "code":
            description += render_code_start(n, is_reference=(tag != "page" and tag \
                != "section"))
            description += _parse_code (n)
            description += render_code_end(n, is_reference=(tag != "page" and tag \
                != "section"))
        elif ctag == "note":
            description += render_note(n)
            add_new_lines = False

        description = _parse_description(n, description, add_new_lines,
                sections_level=sections_level)
        add_new_lines = old_add_new_lines

    description += render_tail(node)
    return description


class Parameter:

    def __init__(self, node):
        self.name = custom_find(node, "title/code").text
        self.description = custom_find(node, "p")


class Page:

    def __init__(self, node):
        self.node = node
        self.name = node.attrib["id"]
        self.description = ""
        links = custom_findall(node, "info/link")
        self.next_ = None
        self.prev_ = None
        for link in links:
            if link.attrib['type'] == 'next':
                self.next_ = link.attrib ["xref"]

class Class (Page):

    def __init__(self, node):
        Page.__init__(self, node)

        self.description = _parse_description(self.node, "")


class Function(Page):

    def __init__(self, node):
        Page.__init__(self, node)
        self.description += self.name

        params = []
        param_nodes = custom_findall(node, "terms/item")
        for n in param_nodes:
            params.append(Parameter(n))

        self.prototype = render_prototype(params)

        self.parameter_descriptions = []
        for param in params:
            self.parameter_descriptions.append(
                render_parameter_description(param))

        self.description = _parse_description(self.node, "")


class AggregatedPages(object):

    def __init__(self):
        self.master_page = None
        self.slave_pages = []

    def add_slave_page(self, page):
        self.slave_pages.append(page)

    def set_master_page(self, page):
        self.master_page = page

    def parse(self, output):
        if self.master_page is None:
            return
        class_ = Class(self.master_page)
        output = os.path.join (output, class_.name + ".markdown")
        functions = dict({})
        with open (output, "w") as f:
            render_class(class_, f)
            for page in self.slave_pages:
                function = Function(page)
                functions[function.name] = function
            functions = self.sort_functions(functions)
            for function in functions:
                render_function(function, f)

    def sort_functions(self, functions):
        sorted_functions = []
        first = None

        for name, function in functions.iteritems():
            if function.next_:
                try:
                    next_ = functions[function.next_]
                    function.next_ = next_
                    next_.prev_ = function
                except KeyError:
                    function.next_ = None
                    continue

        # Find the head, function's list can't contain gaps
        for function in functions.itervalues():
            if not function.prev_ and function.next_:
                first = function
                break

        function = first
        if function:
            sorted_functions.append (function)
            while function.next_:
                sorted_functions.append (function.next_)
                function = function.next_

        for function in functions.itervalues():
            if function not in sorted_functions:
                sorted_functions.append (function)

        return sorted_functions

class Parser(object):

    def __init__(self, files, output):
        self.__pages = {}

        self._trees = dict({})
        for f in files:
            tree = ET.parse(f)
            root = tree.getroot()
            self._parse_page(root)

        for page in self.__pages.values():
            page.parse(output)

    def _parse_page(self, root):
        id_ = root.attrib["id"]
        type_ = root.attrib["style"]
        links = custom_findall(root, "info/link")
        for link in links:
            if link.attrib["type"] == "guide":
                break
        if type_ not in ["class", "method", "function", "constructor"]:
            return
        if "Class" in id_ or "Private" in id_:  # UGLY
            return

        xref = link.attrib["xref"]
        if xref == "index":
            xref = id_

        try:
            pages = self.__pages[xref]
        except KeyError:
            pages = AggregatedPages()
            self.__pages[xref] = pages

        if type_ == "class":
            pages.set_master_page(root)
        else:
            pages.add_slave_page(root)


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-o", "--output",
            action="store", dest="output",
            default="",
            help = "Directory to write output to")
    arg_parser.add_argument ("files", nargs = '+',
            help = "The files to convert to markdown")
    args = arg_parser.parse_args()
    if not args.files:
        print ("please specify files to convert")
        exit (0)

    try:
        ensure_path (args.output)
    except OSError as e:
        print ("The output location is invalid : ", e)
        exit(0)
    parser = Parser(args.files, args.output)
