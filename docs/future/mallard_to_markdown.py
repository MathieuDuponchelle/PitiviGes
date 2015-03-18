#!/usr/bin/env python

import xml.etree.ElementTree as ET
from xml.etree.ElementTree import QName
import argparse
import os
import errno
try:
    import pygraphviz as pg
    HAVE_PYGRAPHVIZ = True
except ImportError:
    HAVE_PYGRAPHVIZ = False

mime_map = {"text/x-csrc": "c",
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
            text = node.text.replace("#", "(FIXME broken link")
        else:
            text = xref
        result = "[" + text + "](#" + xref.lower() + ")"
    elif "href" in node.keys():
        href = node.attrib["href"]
        if node.text:
            text = node.text.replace("#", "(FIXME broken link")
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


def render_code_end(node, is_reference=False):
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


def render_section(node, level):
    title = custom_find(node, "title").text
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


def render_title(title, level=3):
    return "#" * level + title + "\n"

def render_line(line):
    return "%s\n" % line


# Reduced parsing, only look out for links.
def _parse_code(node):
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
        description += render_title(custom_find(node, "title").text, sections_level + 3)

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
            description += render_code_start(n, is_reference=(tag not in
                                                              ["page", "section"]))
            description += _parse_code(n)
            description += render_code_end(n, is_reference=(tag not in ["page",
                                                                        "section"]))
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
        self.synopsis = self.parse_synopsis(node)
        links = custom_findall(node, "info/link")
        self.next_ = None
        self.prev_ = None
        for link in links:
            if link.attrib['type'] == 'next':
                self.next_ = link.attrib["xref"]

    def parse_synopsis(self, node):
        raise NotImplementedError

    def render(self):
        raise NotImplementedError


class Class(Page):
    def __init__(self, node):
        Page.__init__(self, node)

        self.description += _parse_description(self.node, "")
        self.symbols = []
        self.functions = {}

    def walk_hierarchy (self, node, graph, target, parent=None):
        items = custom_findall(node, "item")
        for n in items:
            link = custom_find(n, "link")
            ref=""
            try:
                ref = link.attrib["href"]
            except KeyError:
                try:
                    ref = link.attrib["xref"]
                    ref = "#" + ref.lower()
                except KeyError:
                    pass

            graph.add_node (link.text, URL=ref, style="rounded", shape="box")

            if parent:
                graph.add_edge (parent, link.text)

            if link.text == target:
                return

            items = custom_findall(n, "item")
            if items:
                self.walk_hierarchy(n, graph, target, parent=link.text)
            else:
                graph.add_edge (link.text, target)

    def parse_synopsis(self, node):
        if not HAVE_PYGRAPHVIZ:
            return ""

        graph = pg.AGraph(directed=True, strict=True)
        hierarchy = custom_find(node, "synopsis/tree")
        if hierarchy is None:
            return ""
        self.walk_hierarchy(hierarchy, graph, self.name)
        name = '%s.svg' % (self.name, )
        graph.draw(name, prog="dot")
        result = '<p class="graphviz">\n'
        split = open(name).read().split("\n")[3:]
        os.unlink(name)
        result += ''.join(split)
        result += '</p>'
        return result

    def add_symbol(self, symbol):
        if isinstance(symbol, Function):
            self.functions[symbol.name] = symbol
        else:
            self.symbols.append(symbol)

    def get_symbols(self):
        funcs = self.__get_sorted_functions()

        symbols = self.symbols
        symbols.extend(funcs)

        symbols.sort(key=lambda page: page.PRIORITY)

        return symbols

    def __get_sorted_functions(self):
        sorted_functions = []
        first = None

        for name, function in self.functions.iteritems():
            if function.next_:
                try:
                    next_ = self.functions[function.next_]
                    function.next_ = next_
                    next_.prev_ = function
                except KeyError:
                    function.next_ = None
                    continue

        # Find the head, function's list can't contain gaps
        for function in self.functions.itervalues():
            if not function.prev_ and function.next_:
                first = function
                break

        function = first
        if function:
            sorted_functions.append(function)
            while function.next_:
                sorted_functions.append(function.next_)
                function = function.next_

        for function in self.functions.itervalues():
            if function not in sorted_functions:
                sorted_functions.append(function)

        return sorted_functions

    def render(self):
        out = ""
        out += render_line(render_title(self.name, 2))
        out += render_line(self.synopsis)
        out += self.description
        return out


class Function(Page):
    PRIORITY = 1

    def __init__(self, node, python_node=None):
        Page.__init__(self, node)
        if python_node is not None:
            self.python_synopsis = self.parse_synopsis(python_node)
        else:
            self.python_synopsis = ""
        self.filename = None

        params = []
        param_nodes = custom_findall(node, "terms/item")
        for n in param_nodes:
            params.append(Parameter(n))

        self.prototype = render_prototype(params)

        self.parameter_descriptions = []
        for param in params:
            self.parameter_descriptions.append(
                render_parameter_description(param))

        self.description += _parse_description(self.node, "")

    def parse_synopsis(self, node):
        result = ""
        synopsis = custom_find(node, "synopsis/code")
        if synopsis is not None:
            result += render_code_start(synopsis)
            result += _parse_code(synopsis)
            result += render_code_end(synopsis)

        return result

    def render(self):
        out = render_title(self.name)
        out += render_line(self.synopsis)
        out += render_line(self.python_synopsis)
        for description in self.parameter_descriptions:
            out += render_line(description)

        out += render_line(self.description)

        return out


class Property(Page):
    PRIORITY = 0

    def __init__(self, node):
        Page.__init__(self, node)
        self.description = _parse_description(self.node, "")

    def render(self):
        name = custom_find(self.node, "title").text

        return render_line(render_title(name) + self.description)

    def parse_synopsis(self, node):
        return ""


class AggregatedPages(object):

    def __init__(self):
        self.master_page = None
        self.symbols = []

    def add_slave_page(self, page, filename, python_pages):
        page.filename = filename
        if page.attrib["style"] in ["method", "function", "constructor"]:
            try:
                python_tree = ET.parse(os.path.join(python_pages,
                                                    os.path.basename(page.filename)))
                python_page = python_tree.getroot()
                symbol = Function(page, python_page)
            except IOError:
                symbol = Function(page)
        elif page.attrib["style"] in ["property"]:
            symbol = Property(page)
        else:
            print("Style not handled yet: %s" % page.attrib["style"])
            return

        if not self.master_page:
            self.symbols.append(symbol)
        else:
            self.master_page.add_symbol(symbol)

    def set_master_page(self, page):
        self.master_page = Class(page)
        for s in self.symbols:
            self.master_page.add_symbol(s)

    def parse(self, output):
        if self.master_page is None:
            return

        output = os.path.join(output, self.master_page.name + ".markdown")
        with open(output, "w") as f:
            f.write(self.master_page.render())
            seen_properties = False
            seen_methods = False
            for symbol in self.master_page.get_symbols():
                if not seen_properties and isinstance(symbol, Property):
                    f.write("<h3 id='gobject-properties'><u>GObject properties:</u></h3>\n")
                    seen_properties = True
                elif not seen_methods and isinstance(symbol, Function):
                    f.write("<h3 id='methods'><u>Methods:</u></h3>\n")
                    seen_methods = True
                f.write(symbol.render())


class Parser(object):

    def __init__(self, files, output, python_pages=""):
        self.__pages = {}

        self._trees = dict({})
        for f in files:
            tree = ET.parse(f)
            root = tree.getroot()
            self._parse_page(root, f, python_pages)

        for page in self.__pages.values():
            page.parse(output)

    def _parse_page(self, root, f, python_pages):
        id_ = root.attrib["id"]
        type_ = root.attrib["style"]
        links = custom_findall(root, "info/link")
        for link in links:
            if link.attrib["type"] == "guide":
                break
        if type_ not in ["class", "method", "function", "constructor",
                "property", "interface"]:
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

        if type_ == "class" or type_ == "interface":
            pages.set_master_page(root)
        else:
            pages.add_slave_page(root, f, python_pages)


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-o", "--output",
                            action="store", dest="output",
                            default="",
                            help="Directory to write output to")
    arg_parser.add_argument("-p", "--python-pages",
                            action="store", dest="python_pages",
                            default="",
                            help="Directory to write output to")
    arg_parser.add_argument("files", nargs='+',
                            help="The files to convert to markdown")
    args = arg_parser.parse_args()
    if not args.files:
        print("please specify files to convert")
        exit(0)

    try:
        ensure_path(args.output)
    except OSError as e:
        print("The output location is invalid : ", e)
        exit(0)
    if not HAVE_PYGRAPHVIZ:
        print """
        You don't have pygraphviz, class hierarchy diagrams will not be
        generated
        """
    parser = Parser(args.files, args.output, args.python_pages)
