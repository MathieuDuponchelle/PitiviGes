#!/usr/bin/env python

import xml.etree.ElementTree as ET
from xml.etree.ElementTree import QName
import argparse
import os
import re
import errno
from random import choice
import string

try:
    import pygraphviz as pg
    HAVE_PYGRAPHVIZ = True
except ImportError:
    HAVE_PYGRAPHVIZ = False

mime_map = {"text/x-csrc": "c",
            "text/x-python": "python",
            "application/x-shellscript": "shell"}


class TargetFormats:
    SLATE = "slate"
    MARKDOWN = "markdown"

    @classmethod
    def get_renderer(cls, target_format):
        if target_format == TargetFormats.SLATE:
            return SlateRenderer()
        elif target_format == TargetFormats.MARKDOWN:
            return MarkdownRenderer()

        raise NotImplementedError("Uknown target format: %s" % target_format)


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


class Renderer:
    render_markup = False

    def render_link(self, node):
        raise NotImplementedError

    def render_code_start(self, node, is_reference=False):
        raise NotImplementedError

    def render_code_end(self, node, is_reference=False):
        raise NotImplementedError

    def render_paragraph(self):
        raise NotImplementedError

    def render_section(self, node, level):
        raise NotImplementedError

    def render_text(self, node):
        raise NotImplementedError

    def render_tail(self, node):
        raise NotImplementedError

    def render_prototype(self, params):
        raise NotImplementedError

    def render_parameter_description(self, param):
        raise NotImplementedError

    def render_note(self, node):
        return "\n\n> "

    def render_title(self, title, level=3, c_name=None, python_name=None,
                     shell_name=None):
        raise NotImplementedError

    def render_line(self, line):
        raise NotImplementedError

    def render_subsection(self, id, name):
        raise NotImplementedError


class MarkdownRenderer(Renderer):
    def render_link(self, node):
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

    def render_code_start(self, node, is_reference=False):
        mime = ""
        if "mime" in node.attrib:
            mime = node.attrib["mime"]
        if mime in mime_map:
            mime = mime_map[mime]
        else:
            mime = ""

        result = ""

        if not is_reference:
            result += "\n\n```" + mime
        else:
            result += "**"
        return result

    def render_code_end(self, node, is_reference=False):
        mime = ""
        result = ""

        if "mime" in node.attrib:
            mime = node.attrib["mime"]
        if mime in mime_map:
            mime = mime_map[mime]
        else:
            mime = ""
        if not is_reference:
            result += "\n```"
        else:
            result += "**"

        if node.tail:
            result += node.tail
        return result

    def render_paragraph(self):
        return "\n\n"

    def render_section(self, node, level):
        title = custom_find(node, "title").text
        return "\n\n###" + level * "#" + title + "\n"

    def render_text(self, node):
        if node.text:
            return node.text.replace("#", "(FIXME broken link)")
        return ""

    def render_tail(self, node):
        if node.tail:
            return node.tail.replace("#", "(FIXME broken link)")
        return ""

    def render_prototype(self, params):
        result = "("
        for param in params:
            if param.name == "Returns":
                break
            if params[0] != param:
                result += ", "
            result += param.name
        result += ")"
        return result

    def render_parameter_description(self, param):
        result = "*" + param.name + "*: "
        if param.description is not None:
            result += _parse_description(
                param.description, self, "", add_new_lines=False)
        else:
            result += "FIXME empty description\n"
        return result

    def render_note(self, node):
        return "\n\n> "

    def render_title(self, title, level=3, c_name=None, python_name=None,
                     shell_name=None):

        if True:  # FIXME do the thinkg
            return "#" * level + title

    def render_line(self, line):
        return "%s\n" % line

    def render_subsection(self, id, name):
        return "###%s\n\n" % (name)


class SlateRenderer(MarkdownRenderer):
    render_markup = False

    def render_title(self, title, level=3, c_name=None, python_name=None,
                     shell_name=None):

        res = "<h" + str(level) + ' id="' + title.lower() + '"'
        if c_name:
            res += ' c_name="' + c_name + '"'
        if python_name:
            res += ' python_name="' + python_name + '"'
        if shell_name:
            res += ' shell_name="' + shell_name + '"'
        res += '>'
        res += title
        res += "</h" + str(level) + ">\n"
        return res

    def render_code_end(self, node, is_reference=False):
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

    def render_code_start(self, node, is_reference=False):
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

    def render_subsection(self, id, name):
        chars = string.letters
        rand_name = lambda x: ''.join(choice(chars) for i in range(x))
        return "<h3 id='%s' class='subsection'><u>%s</u></h3>\n" % (rand_name(6), name)


# Reduced parsing, only look out for links.
def _parse_code(node, renderer):
    description = ""

    if node.text:
        description += node.text
    for n in node:
        ctag = n.tag.split('}')[1]
        if ctag == "link":
            description += renderer.render_link(n)
    return description


def _parse_description(node, renderer, description, add_new_lines=True, sections_level=0):
    tag = node.tag.split('}')[1]
    if tag not in ["page", "p", "section", "note"]:
        return description

    if tag == "section":
        sections_level += 1
        description += renderer.render_title(custom_find(node, "title").text, sections_level + 3)

    if node.text:
        if add_new_lines:
            description += renderer.render_paragraph()

    description += renderer.render_text(node)

    for n in node:
        old_add_new_lines = add_new_lines
        ctag = n.tag.split('}')[1]
        if ctag == "link":
            description += renderer.render_link(n)
        elif ctag == "code":
            description += renderer.render_code_start(n, is_reference=(tag not in
                                                                       ["page", "section"]))
            description += _parse_code(n, renderer)
            description += renderer.render_code_end(n, is_reference=(tag not in ["page",
                                                                     "section"]))
        elif ctag == "note":
            description += renderer.render_note(n)
            add_new_lines = False

        description = _parse_description(n, renderer, description, add_new_lines,
                                         sections_level=sections_level)
        add_new_lines = old_add_new_lines

    description += renderer.render_tail(node)
    return description


class Parameter:

    def __init__(self, parent, node):
        self.name = custom_find(node, "title/code").text
        self.description = custom_find(node, "p")

        self.valid = True
        if self.name == "Returns" and self.description is None:
            if custom_find(parent, "synopsis/code").text.replace("\n", "").split(" ")[0] == "void":
                self.valid = False


class Page:
    def __init__(self, node, renderer):
        self.node = node
        self.renderer = renderer
        self.name = node.attrib["id"]
        self.description = ""
        self.synopsis = self.parse_synopsis(node)
        links = custom_findall(node, "info/link")
        self.next_ = None
        self.prev_ = None

        for link in links:
            if link.attrib['type'] == 'next':
                self.next_ = link.attrib["xref"]

    def get_code_languages (self):
        languages = set({})
        code_samples = self.node.findall(".//{http://projectmallard.org/1.0/}code")
        for sample in code_samples:
            try:
                mime = sample.attrib["mime"]
            except KeyError:
                continue
            try:
                language = mime_map[mime]
            except KeyError:
                continue
            languages.add(language)

        return languages

    def parse_synopsis(self, node):
        raise NotImplementedError

    def render(self, languages=None):
        raise NotImplementedError


class Class(Page):
    def __init__(self, node, renderer):
        Page.__init__(self, node, renderer)

        self.description += _parse_description(self.node, self.renderer, "")
        self.symbols = []
        self.functions = {}

    def walk_hierarchy(self, node, graph, target, parent=None):
        items = custom_findall(node, "item")
        for n in items:
            link = custom_find(n, "link")
            ref = ""
            try:
                ref = link.attrib["href"]
            except KeyError:
                try:
                    ref = link.attrib["xref"]
                    ref = "#" + ref.lower()
                except KeyError:
                    pass

            graph.add_node(link.text, URL=ref, style="rounded", shape="box")

            if parent:
                graph.add_edge(parent, link.text)

            if link.text == target:
                return

            items = custom_findall(n, "item")
            if items:
                self.walk_hierarchy(n, graph, target, parent=link.text)
            else:
                graph.add_edge(link.text, target)

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

    def render(self, languages):
        out = ""
        c_name = re.sub("[.]", "", self.name)
        if "python" in languages:
            python_name = self.name
        else:
            python_name = None
        if "shell" in languages:
            shell_name = re.sub("GES.", "The", self.name)
            shell_name = re.sub('([a-z0-9])([A-Z])', r'\1 \2', shell_name)
        else:
            shell_name = None

        out += self.renderer.render_line(self.renderer.render_title(self.name, level=2, c_name=c_name,
                                            python_name=python_name,
                                            shell_name=shell_name))
        if self.renderer.render_markup:
            out += self.renderer.render_line(self.synopsis)
        out += self.description
        return out


class Function(Page):
    PRIORITY = 1

    def __init__(self, node, renderer, python_node=None):
        Page.__init__(self, node, renderer)
        self.languages = None
        self.python_node = python_node
        if python_node is not None:
            self.python_synopsis = self.parse_synopsis(python_node)
        else:
            self.python_synopsis = ""
        self.filename = None

        params = []
        param_nodes = custom_findall(node, "terms/item")
        f = True
        for n in param_nodes:
            param = Parameter(node, n)
            if f:
                param.name = "self"
                f = False

            if param.valid:
                params.append(param)

        self.prototype = self.renderer.render_prototype(params)

        self.parameter_descriptions = []
        for param in params:
            self.parameter_descriptions.append(
                self.renderer.render_parameter_description(param))

        self.description += _parse_description(self.node, self.renderer, "")

    def get_code_languages (self):
        languages = Page.get_code_languages(self)
        if self.python_node is not None:
            languages.add("python")
        return languages

    def parse_synopsis(self, node):
        result = ""
        synopsis = custom_find(node, "synopsis/code")
        if synopsis is not None:
            result += self.renderer.render_code_start(synopsis)
            result += _parse_code(synopsis, self.renderer)
            result += self.renderer.render_code_end(synopsis)

        return result

    def get_c_name(self):
        c_name = re.sub("[.]", "_", self.name)
        c_name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', c_name)
        c_name = re.sub('([a-z0-9])([A-Z])', r'\1_\2', c_name).lower()
        c_name = re.sub('__', r'_', c_name).lower()

        return c_name

    def render(self):
        languages = self.get_code_languages()
        c_name = self.get_c_name()
        if "python" in languages:
            python_name = self.name
        else:
            python_name = None
        if "shell" in languages:
            shell_name = self.name
        else:
            shell_name = None

        out = self.renderer.render_title(self.name, c_name=c_name,
                python_name=python_name, shell_name=shell_name)
        out += self.renderer.render_line(self.synopsis)
        out += self.renderer.render_line(self.python_synopsis)
        for description in self.parameter_descriptions:
            out += self.renderer.render_line(description)

        out += self.renderer.render_line(self.description)

        return out

class VirtualFunction(Function):
    PRIORITY = 2

    def __init__(self, node, renderer, python_node=None):
        Function.__init__(self, node, renderer, python_node)
        self.name = self.name.replace("-", ".do_")

    def get_c_name(self):
        return self.name.replace(".do_", "Class:").replace('.', '')

    def parse_synopsis(self, node):
        result = ""
        synopsis = custom_find(node, "synopsis/code")
        if synopsis is not None:
            result += self.renderer.render_code_start(synopsis)
            if mime_map[synopsis.attrib["mime"]] == "c":
                code = synopsis.text
                tmp = code.split(" ")
                tmp[1] = self.name.replace("-", "Class->").replace('.', '')
                code = "\n" + ' '.join(tmp).replace("\n", "\n" + "      ").lstrip()
                result += self.renderer.render_line(code)
            else:
                result += _parse_code(synopsis, self.renderer)
            result += self.renderer.render_code_end(synopsis)

        return result

class Property(Page):
    PRIORITY = 0

    def __init__(self, node, renderer, python_node=None):
        Page.__init__(self, node, renderer)
        self.python_node = python_node
        self.description = _parse_description(self.node, self.renderer, "")

    def get_code_languages (self):
        languages = Page.get_code_languages(self)
        if self.python_node is not None:
            languages.add("python")
        return languages

    def render(self):
        name = custom_find(self.node, "title").text

        languages = self.get_code_languages()
        c_name = name
        if "python" in languages:
            python_name = name
        else:
            python_name = None
        if "shell" in languages:
            shell_name = self.name
        else:
            shell_name = None

        return self.renderer.render_line(self.renderer.render_title(name,
            c_name=c_name, python_name=python_name, shell_name=shell_name) + self.description)

    def parse_synopsis(self, node):
        return ""


class AggregatedPages(object):

    def __init__(self, renderer):
        self.master_page = None
        self.symbols = []
        self.renderer = renderer
        self.languages = set({})

    def add_slave_page(self, page, filename, python_pages):
        page.filename = filename
        try:
            python_tree = ET.parse(os.path.join(python_pages,
                                                    os.path.basename(page.filename)))
            python_page = python_tree.getroot()
        except IOError:
            python_page = None

        if page.attrib["style"] in ["method", "function", "constructor"]:
            symbol = Function(page, self.renderer, python_page)
        elif page.attrib["style"] in ["property"]:
            symbol = Property(page, self.renderer, python_page)
        elif page.attrib["style"] in ["vfunc"]:
            symbol = VirtualFunction(page, self.renderer, python_page)
        else:
            print("Style not handled yet: %s" % page.attrib["style"])
            return

        self.languages = self.languages.union(symbol.get_code_languages())
        if not self.master_page:
            self.symbols.append(symbol)
        else:
            self.master_page.add_symbol(symbol)

    def set_master_page(self, page, filename, python_pages):
        self.master_page = Class(page, self.renderer)
        if os.path.exists(os.path.join(python_pages,
            os.path.basename(filename))):
            self.languages.add("python")
        self.languages = self.languages.union(self.master_page.get_code_languages())
        for s in self.symbols:
            self.master_page.add_symbol(s)

    def languages_for_priority(self, symbols, priority):
        languages = set({})
        for symbol in symbols:
            if symbol.PRIORITY == priority:
                languages = languages.union(symbol.get_code_languages())
        return languages

    def parse(self, output):
        if self.master_page is None:
            return

        output = os.path.join(output, self.master_page.name + ".markdown")
        with open(output, "w") as f:
            res = self.master_page.render(self.languages)
            seen_properties = False
            seen_methods = False
            seen_vfuncs = False
            for symbol in self.master_page.get_symbols():
                if not seen_properties and isinstance(symbol, Property):
                    res += self.renderer.render_subsection('gobject-properties', "GObject properties:")
                    seen_properties = True
                elif not seen_methods and isinstance(symbol, Function):
                    res += self.renderer.render_subsection('methods', "Methods:")
                    seen_methods = True
                elif not seen_vfuncs and isinstance(symbol, VirtualFunction):
                    res += self.renderer.render_subsection('vfuncs', "Virtual Methods:")
                    seen_vfuncs = True
                res += symbol.render()

            clean_res = ""
            n_newline = 0
            for l in res.split("\n"):
                l = re.sub("^( )*$", "", l)
                if l == "":
                    n_newline += 1
                    if n_newline > 1:
                        continue
                else:
                    n_newline = 0
                clean_res += "%s\n" % l
            f.write(clean_res)


class Parser(object):

    def __init__(self, files, output, renderer, python_pages=""):
        self.__pages = {}
        self._renderer = renderer

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
                         "property", "interface", "vfunc"]:
            return

        if "Class" in id_ or "Private" in id_:  # UGLY
            return

        xref = link.attrib["xref"]
        if xref == "index":
            xref = id_

        try:
            pages = self.__pages[xref]
        except KeyError:
            pages = AggregatedPages(self._renderer)
            self.__pages[xref] = pages

        if type_ == "class" or type_ == "interface":
            pages.set_master_page(root, f, python_pages)
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
    arg_parser.add_argument('-t', "--target-format", dest='target_format',
                            default="markdown",
                            help="The target format to be generated (supported: 'markdown', 'slate')")
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
    parser = Parser(args.files, args.output, TargetFormats.get_renderer(args.target_format), args.python_pages)
