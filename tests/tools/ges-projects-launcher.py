#!/usr/bin/python
import re
import os
import sys
import time
import signal
import traceback
import subprocess
import codecs
from xml.sax import saxutils
from optparse import OptionParser
from gi.repository import GES, Gst, GLib
from urllib import unquote
from urlparse import urlsplit

DURATION_TOLERANCE = Gst.SECOND / 2
MAX_PLAYBACK_DURATION_MULTIPLIER = 2
UNICODE_STRINGS = (type(unicode()) == type(str()))
DEFAULT_ASSET_REPO = "https://github.com/pitivi/projects"

APPLICATION = "ges-launch-1.0"


class Colors(object):
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

    def desactivate(self):
        Colors.HEADER = ''
        Colors.OKBLUE = ''
        Colors.OKGREEN = ''
        Colors.WARNING = ''
        Colors.FAIL = ''
        Colors.ENDC = ''


class Combination(object):

    def __str__(self):
        return "%s and %s in %s" % (self.acodec, self.vcodec, self.container)

    def __init__(self, container, acodec, vcodec):
        self.container = container
        self.acodec = acodec
        self.vcodec = vcodec


FORMATS = {"aac": "audio/mpeg,mpegversion=4",
           "ac3": "audio/x-ac3",
           "vorbis": "audio/x-vorbis",
           "mp3": "audio/mpeg,mpegversion=1,layer=3",
           "h264": "video/x-h264",
           "vp8": "video/x-vp8",
           "theora": "video/x-theora",
           "ogg": "application/ogg",
           "mkv": "video/x-matroska",
           "mp4": "video/quicktime,variant=iso;",
           "webm": "video/x-matroska"}

COMBINATIONS = [
    Combination("ogg", "vorbis", "theora"),
    Combination("webm", "vorbis", "vp8"),
    Combination("mp4", "mp3", "h264"),
    Combination("mkv", "vorbis", "h264")]

SCENARIOS = ["none", "seek_forward", "seek_backward", "scrub_forward_seeking"]


def print_ns(timestamp):
    if timestamp == Gst.CLOCK_TIME_NONE:
        return "CLOCK_TIME_NONE"

    return str(timestamp / (Gst.SECOND * 60 * 60)) + ':' + \
        str((timestamp / (Gst.SECOND * 60)) % 60) + ':' + \
        str((timestamp / Gst.SECOND) % 60) + ':' + \
        str(timestamp % Gst.SECOND)


def get_profile_full(muxer, venc, aenc, video_restriction=None,
                     audio_restriction=None,
                     audio_presence=0, video_presence=0):
    ret = "\""
    if muxer:
        ret += muxer
    ret += ":"
    if venc:
        if video_restriction is not None:
            ret = ret + video_restriction + '->'
        ret += venc
        if video_presence:
            ret = ret + '|' + str(video_presence)
    if aenc:
        ret += ":"
        if audio_restriction is not None:
            ret = ret + audio_restriction + '->'
        ret += aenc
        if audio_presence:
            ret = ret + '|' + str(audio_presence)

    ret += "\""
    return ret.replace("::", ":")


def get_profile(combination):
    return get_profile_full(FORMATS[combination.container],
                            FORMATS[combination.vcodec],
                            FORMATS[combination.acodec],
                            video_restriction="video/x-raw,format=I420")


def get_project_duration(project_uri):

    proj = GES.Project.new(project_uri)
    tl = proj.extract()
    if tl is None:
        duration = None
    else:
        duration = tl.get_meta("duration")
    if duration is not None:
        return duration / Gst.SECOND
    return 2 * 60

# Invalid XML characters, control characters 0-31 sans \t, \n and \r
CONTROL_CHARACTERS = re.compile(r"[\000-\010\013\014\016-\037]")

TEST_ID = re.compile(r'^(.*?)(\(.*\))$')


def xml_safe(value):
    """Replaces invalid XML characters with '?'."""
    return CONTROL_CHARACTERS.sub('?', value)


def escape_cdata(cdata):
    """Escape a string for an XML CDATA section."""
    return xml_safe(cdata).replace(']]>', ']]>]]&gt;<![CDATA[')


def id_split(idval):
    m = TEST_ID.match(idval)
    if m:
        name, fargs = m.groups()
        head, tail = name.rsplit(".", 1)
        return [head, tail + fargs]
    else:
        return idval.rsplit(".", 1)


def quote_uri(uri):
    """
    Encode a URI/path according to RFC 2396, without touching the file:/// part.
    """
    # Split off the "file:///" part, if present.
    parts = urlsplit(uri, allow_fragments=False)
    # Make absolutely sure the string is unquoted before quoting again!
    raw_path = unquote(parts.path)
    # For computing thumbnail md5 hashes in the media library, we must adhere to
    # RFC 2396. It is quite tricky to handle all corner cases, leave it to Gst:
    return Gst.filename_to_uri(raw_path)


def exc_message(exc_info):
    """Return the exception's message."""
    exc = exc_info[1]
    if exc is None:
        # str exception
        result = exc_info[0]
    else:
        try:
            result = str(exc)
        except UnicodeEncodeError:
            try:
                result = unicode(exc)
            except UnicodeError:
                # Fallback to args as neither str nor
                # unicode(Exception(u'\xe6')) work in Python < 2.6
                result = exc.args[0]
    return xml_safe(result)


class Xunit():
    """This plugin provides test results in the standard XUnit XML format."""
    name = 'xunit'
    encoding = 'UTF-8'
    xml_file = None

    def __init__(self, options):
        self._capture_stack = []
        self._currentStdout = None
        self._currentStderr = None
        self.configure(options)
        open("/tmp/gesintergrationstdout", 'w').close()
        open("/tmp/gesintergrationstderr", 'w').close()

    def _timeTaken(self):
        if hasattr(self, '_timer'):
            taken = time.time() - self._timer
        else:
            # test died before it ran (probably error in setup())
            # or success/failure added before test started probably
            # due to custom TestResult munging
            taken = 0.0
        return taken

    def _quoteattr(self, attr):
        """Escape an XML attribute. Value can be unicode."""
        attr = xml_safe(attr)
        if isinstance(attr, unicode) and not UNICODE_STRINGS:
            attr = attr.encode(self.encoding)
        return saxutils.quoteattr(attr)

    def configure(self, options):
        """Configures the xunit plugin."""
        self.stats = {'errors': 0,
                      'failures': 0,
                      'passes': 0,
                      'skipped': 0
                      }
        self.errorlist = []
        self.results = []
        self.xuint_file_path = options.xunit_file

    def report(self):
        """Writes an Xunit-formatted XML file

        The file includes a report of test errors and failures.

        """
        print "Writing XML file to: %s" % self.xuint_file_path
        self.xml_file = codecs.open(self.xuint_file_path, 'w',
                                    self.encoding, 'replace')
        self.stats['encoding'] = self.encoding
        self.stats['total'] = (self.stats['errors'] + self.stats['failures']
                               + self.stats['passes'] + self.stats['skipped'])
        self.xml_file.write( u'<?xml version="1.0" encoding="%(encoding)s"?>'
            u'<testsuite name="gesprojectslauncher" tests="%(total)d" '
            u'errors="%(errors)d" failures="%(failures)d" '
            u'skip="%(skipped)d">' % self.stats)
        self.xml_file.write(u''.join([self._forceUnicode(e)
                            for e in self.errorlist]))
        self.xml_file.write(u'</testsuite>')
        self.xml_file.close()

    def _start_capture(self):
        self._capture_stack.append((sys.stdout, sys.stderr))
        self._currentStdout = open("/tmp/gesintergrationstdout", 'r')
        self._currentStderr = open("/tmp/gesintergrationstderr", 'r')

    def start_context(self, context):
        self._start_capture()

    def before_test(self):
        """Initializes a timer before starting a test."""
        self._timer = time.time()
        self._start_capture()

    def end_capture(self):
        if self._capture_stack:
            sys.stdout, sys.stderr = self._capture_stack.pop()

    def after_test(self):
        self.end_capture()
        self._currentStdout.close()
        self._currentStderr.close()
        self._currentStdout = None
        self._currentStderr = None

    def finalize(self):
        while self._capture_stack:
            self.end_capture()

    def _get_captured_stdout(self):
        if self._currentStdout:
            self._currentStdout.seek(0)
            value = self._currentStdout.read()
            if value:
                return '<system-out><![CDATA[%s]]></system-out>' % \
                    escape_cdata(value)
        return ''

    def _get_captured_stderr(self):
        if self._currentStderr:
            self._currentStderr.seek(0)
            value = self._currentStderr.read()
            if value:
                return '<system-err><![CDATA[%s]]></system-err>' \
                    % escape_cdata(value)
        return ''

    def add_error(self, test, err, exc=None, capt=None):
        """Add error output to Xunit report.
        """
        taken = self._timeTaken()

        if err[0] == "skipped":
            type = 'skipped'
            self.stats['skipped'] += 1
        else:
            type = 'error'
            self.stats['errors'] += 1

        if exc is not None:
            tb = traceback.format_exc(exc)
        else:
            tb = ""
        self.results.insert(0, test)
        self.errorlist.append(
            '<testcase classname=%(cls)s name=%(name)s time="%(taken).3f">'
            '<%(type)s type=%(errtype)s message=%(message)s><![CDATA[%(tb)s]]>'
            '</%(type)s>%(systemout)s%(systemerr)s</testcase>' %
            {'cls': self._quoteattr(test.classname),
             'name': self._quoteattr(test.name),
             'taken': taken,
             'type': type,
             'errtype': self._quoteattr(err),
             'message': self._quoteattr(exc_message(err)),
             'tb': escape_cdata(tb),
             'systemout': self._get_captured_stdout(),
             'systemerr': self._get_captured_stderr(),
             })

    def add_failure(self, test, message, err, exc=None, capt=None):
        """Add failure output to Xunit report.
        """
        taken = self._timeTaken()
        if exc is not None:
            tb = traceback.print_exc(exc)
        else:
            tb = ""

        self.stats['failures'] += 1
        self.results.insert(0, test)
        self.errorlist.append(
            '<testcase classname=%(cls)s name=%(name)s time="%(taken).3f">'
            '<failure type=%(errtype)s message=%(message)s><![CDATA[%(tb)s]]>'
            '</failure>%(systemout)s%(systemerr)s</testcase>' %
            {'cls': self._quoteattr(test.classname),
             'name': self._quoteattr(test.name),
             'taken': taken,
             'errtype': self._quoteattr(err),
             'message': self._quoteattr(message),
             'tb': escape_cdata(tb),
             'systemout': self._get_captured_stdout(),
             'systemerr': self._get_captured_stderr(),
             })

    def add_success(self, test, capt=None):
        """Add success output to Xunit report.
        """
        taken = self._timeTaken()
        self.stats['passes'] += 1
        self.results.append(test)
        self.errorlist.append(
            '<testcase classname=%(cls)s name=%(name)s '
            'time="%(taken).3f">%(systemout)s%(systemerr)s</testcase>' %
            {'cls': self._quoteattr(test.classname),
             'name': self._quoteattr(test.name),
             'taken': taken,
             'systemout': self._get_captured_stdout(),
             'systemerr': self._get_captured_stderr(),
             })

    def _forceUnicode(self, s):
        if not UNICODE_STRINGS:
            if isinstance(s, str):
                s = s.decode(self.encoding, 'replace')
        return s


class ProjectTest(object):
    def __init__(self, classname, project_uri, reporter, options):
        self.name = os.path.basename(project_uri)
        self.classname = classname
        self.scenario = "none"
        self.reporter = reporter
        self.project_uri = project_uri
        self.options = options
        self.command = "%s -l %s" % (APPLICATION, project_uri)
        self.result = False

        # playback props
        self.is_playback = False

        # rendering props
        self.dest_uri = None
        self.dest_file = None
        self.combination = None
        self.is_rendering = False

    def __str__(self):
        string = self.classname
        if self.result:
            string += ": " + self.result
            if "FAILED" in self.result:
                string += "\n       You can reproduce with: " + self.command

        return string

    def set_rendering_info(self, combination, dest_uri):
        self.combination = combination
        self.dest_uri = dest_uri

        if not Gst.uri_is_valid(dest_uri):
            self.dest_uri = GLib.filename_to_uri(dest_uri, None)

        self.dest_file = os.path.join(dest_uri,
                                      os.path.basename(self.project_uri) +
                                      '-' + combination.acodec +
                                      combination.vcodec + '.' +
                                      combination.container)
        profile = get_profile(combination)
        self.command_add_args("-f", profile, "-o", self.dest_file)
        self.command_add_args("--set-scenario", "none")

    def set_playback_infos(self, scenario):
        self.scenario = scenario
        self.command_add_args("--set-scenario", scenario)

    def set_sample_paths(self, options, project_uri):
        if not options.paths:
            if not options.recurse_paths:
                return
            paths = [os.path.dirname(Gst.uri_get_location(project_uri))]
        else:
            paths = options.paths

        for path in paths:
            if options.recurse_paths:
                self.command_add_args("--sample-paths", quote_uri(path))
                for root, dirs, files in os.walk(path):
                    for directory in dirs:
                        self.command_add_args("--sample-paths", quote_uri(os.path.join(path, root, directory)))
            else:
                self.command_add_args("--sample-paths", "file://" + path)

    def command_add_args(self, *args):
        for arg in args:
            self.command += " " + arg

    def get_last_position(self):
        self.reporter._currentStdout.seek(0)
        m = None
        for l in self.reporter._currentStdout.readlines():
            if "<Position:" in l:
                m = l

        if m is None:
            return ""

        pos = ""
        for j in m.split("\r"):
            if j.startswith("<Position:") and j.endswith("/>"):
                pos = j

        return pos

    def get_criticals(self):
        self.reporter._currentStdout.seek(0)
        ret = "["
        for l in self.reporter._currentStdout.readlines():
            if "critical : " in l:
                if ret != "[":
                    ret += ", "
                ret += l.split("critical : ")[1].replace("\n", '')

        if ret == "[":
            return "No critical"
        else:
            return ret + "]"

    def _find_core_file(self):
        cwd = os.path.abspath(os.curdir)
        files = os.listdir(cwd)
        for fname in files:
            if fname == "core":
                return os.path.join(cwd, fname)
            # process.pid is the shell pid, let's just get the next
            # one so *most probobly* get the right one.
            if fname == "core.%d" % (self.process.pid + 1):
                return os.path.join(cwd, fname)
        return None

    def _get_bin_prog(self):
        prog = subprocess.check_output("which " + APPLICATION,
                                       shell=True).replace("\n", '')

        if "gst-editing-services/tools/" in prog:
            binprog = os.path.expanduser(prog.replace(APPLICATION,
                                         ".libs/lt-" + APPLICATION))
        else:
            binprog = os.path.expanduser(prog)

        return binprog

    def get_backtrace(self, reason):
        if self.options.gdb is False:
            return

        # process.pid is the shell pid, let's just get the next
        # one so *most probobly* get the right one.
        arg = None
        if self.process.returncode is None:
            arg = self.process.pid + 1
        else:
            core = self._find_core_file()
            if core:
                arg = os.path.abspath(core)

        if arg is not None:
            print "\n%s============================" % Colors.FAIL
            print " %s- Getting backtrace " % reason
            print "============================%s\n" % Colors.ENDC
            binprog = self._get_bin_prog()
            command = "echo 0 | gdb" \
                      " -ex 'thread apply all backtrace'" \
                      " -ex 'quit'"  \
                      " %s %s" % (binprog, arg)
            print "Launching %s" % command
            os.system(command)
            print "\n============================\n"
        else:
            print "Can not get backtrace"

    def run(self):
        timeout = False
        last_val = 0
        last_change_ts = time.time()
        self.duration = get_project_duration(self.project_uri)
        self.reporter.before_test()
        print "Launching %s" % self.command
        command = "set -o pipefail &&" + self.command + " 2>&1 |tee /tmp/gesintergrationstdout"
        try:
            self.process = subprocess.Popen(command, stderr=sys.stderr,
                                            stdout=sys.stdout,
                                            shell=True,
                                            preexec_fn=os.setsid)
            while True:
                self.process.poll()
                if self.process.returncode is not None:
                    break

                # Dirty way to avoid eating to much CPU... good enough for us anyway.
                time.sleep(1)

#                if self.options.gdb is True:
#                    continue

                if self.is_rendering:
                    val = os.stat(GLib.filename_from_uri(self.dest_file)[0]).st_size
                else:
                    val = self.get_last_position()

                if val == last_val:
                    if time.time() - last_change_ts > 10:
                        timeout = True
                        self.get_backtrace("TIMEOUT")
                        os.killpg(self.process.pid, signal.SIGTERM)
                        break
                else:
                    last_change_ts = time.time()
                    last_val = val

        except KeyboardInterrupt:
            os.killpg(self.process.pid, signal.SIGTERM)
            raise

        result = self._check_result(timeout)
        self.reporter.after_test()

        return result

    def _set_result(self, msg=None, error=None, exc=None):
        if error:
            self.reporter.add_failure(self, msg, error, exc)
            self.result = "%sFAILED: %s%s" % (Colors.FAIL, msg, Colors.ENDC)
        else:
            self.reporter.add_success(self)
            self.result = "%sPASSED%s" % (Colors.OKGREEN, Colors.ENDC)

        print str(self) + "\n"

    def _check_result(self, timeout):
        ret = True
        if self.process.returncode == 0:
            if self.is_rendering:
                try:
                    asset = GES.UriClipAsset.request_sync(self.dest_file)
                    if self.duration - DURATION_TOLERANCE <= asset.get_duration() \
                            <= self.duration + DURATION_TOLERANCE:
                        self._set_result("Duration of encoded file is "
                                        " wrong (%s instead of %s)" %
                                        (print_ns(self.duration),
                                         print_ns(asset.get_duration())),
                                        "wrong-duration")
                        ret = False
                except GLib.Error as e:
                    self._set_result("Wrong rendered file", "failure", e)
                    ret = False
                else:
                    self._set_result()
            else:
                self._set_result()
        else:
            if self.is_rendering and timeout is True:
                missing_eos = False
                try:
                    asset = GES.UriClipAsset.request_sync(self.dest_file)
                    if asset.get_duration() == self.duration:
                        missing_eos = True
                except Exception as e:
                    pass

                if missing_eos is True:
                    self._set_result("The rendered file add right duration, MISSING EOS?\n",
                                     "failure", e)
                else:
                    self._set_result("", "failure", e)
                ret = False
            else:
                if timeout:
                    self._set_result("Application timed out", "timeout")
                else:
                    if self.process.returncode == 139:
                        self.get_backtrace("SEGFAULT")
                        self._set_result("Application segfaulted")
                    else:
                        self._set_result("Application returned %d (issues: %s)" \
                                         % (self.process.returncode, self.get_criticals()),
                                         "error")
                ret = False

        return ret


def launch_project(project_uri, classname, options, scenario="none", combination=None, dest_uri=None, xunit=None):
    playback_test = combination is None
    test = ProjectTest(classname, project_uri, xunit, options)
    if playback_test:
        test.set_playback_infos(scenario)
    else:
        test.set_rendering_info(combination, dest_uri)

    test.set_sample_paths(options, project_uri)

    if options.mute:
        test.command_add_args(" --mute")

    print Colors.HEADER + len(str(test)) * '='
    print str(test)
    print len(str(test)) * '=' + Colors.ENDC

    res = test.run()
    os.system("killall lt-ges-launch-1.0")
    return res


def is_test_wanted(patterns, classname):
    if not patterns:
        return True

    for pattern in patterns:
        if pattern.findall(classname):
            return True

    return False

def report_final(xunit):

    xunit.report()
    msg = "Final report: %d/%d PASSED, %d/%d FAILED" % \
        (xunit.stats['passes'], xunit.stats['total'],
         xunit.stats['failures'], xunit.stats['total'])
    print ""
    print Colors.HEADER + len(msg) * '='
    print msg
    print len(msg) * '=' + Colors.ENDC
    for res in xunit.results:
        print "%s\n" % res

    return xunit.stats['failures']

if "__main__" == __name__:
    PARSER = OptionParser()
    default_opath = GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_VIDEOS)
    if default_opath:
        default_path = os.path.join(default_opath, "ges-projects")
    else:
        default_path = os.path.join(os.path.expanduser('~'), "Video",
                                    "ges-projects")

    PARSER.add_option("-o", "--output-path", dest="dest",
                      default=os.path.join(default_path, "rendered"),
                      help="Set the path to which projects should be renderd")
    PARSER.add_option("-P", "--sample-path", dest="paths",
                      default=[],
                      help="Paths in which to look for moved assets")
    PARSER.add_option("-g", "--gdb", dest="gdb",
                      action="store_true",
                      default=False,
                      help="Run %s into gdb" % APPLICATION)
    PARSER.add_option("-r", "--recurse-paths", dest="recurse_paths",
                      default=False, action="store_true",
                      help="Whether to recurse into paths to find assets")
    PARSER.add_option("-m", "--mute", dest="mute",
                      action="store_true", default=False,
                      help="Mute playback output, which mean that we use faksinks")
    PARSER.add_option("-f", "--forever", dest="forever",
                      action="store_true", default=False,
                      help="Keep running tests until one fails")
    PARSER.add_option("-F", "--fatal-error", dest="fatal_error",
                      action="store_true", default=False,
                      help="Stop on first fail")
    PARSER.add_option(
        '--xunit-file', action='store',
        dest='xunit_file', metavar="FILE",
        default=os.environ.get('XUNIT_FILE', 'gesintegrationtests.xml'),
        help=("Path to xml file to store the xunit report in. "
              "Default is gesintegrationtests.xml in the working directory "))
    PARSER.add_option("-t", "--wanted-tests", dest="wanted_tests",
                      default=None,
                      help="Define the tests to execute, it can be a regex")
    (options, args) = PARSER.parse_args()

    Gst.init(None)
    GES.init()

    if not args and not os.path.exists(default_path):
        print "%s==============" % Colors.HEADER
        print "Getting assets"
        print "==============%s" % Colors.ENDC
        os.system("git clone %s %s" % (DEFAULT_ASSET_REPO,
                    default_path))

    if not Gst.uri_is_valid(options.dest):
        options.dest = GLib.filename_to_uri(options.dest, None)

    print "Creating directory: %s" % options.dest
    try:
        os.makedirs(GLib.filename_from_uri(options.dest)[0])
        print "Created directory: %s" % options.dest
    except OSError:
        pass

    # Let's try to get a backtrace on segfault
    if options.gdb:
        try:
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (-1, -1))
        except:
            print "Couldn't change core limit, no backtrace on segfault"

    projects = list()
    if not args:
        options.paths = [os.path.join(default_path, "assets")]
        path = os.path.join(default_path, "projects")
        for root, dirs, files in os.walk(path):
            for f in files:
                if not f.endswith(".xges"):
                    continue

                projects.append(GLib.filename_to_uri(os.path.join(path, root, f), None))
    else:
        for proj in args:
            if Gst.uri_is_valid(proj):
                projects.append(proj)
            else:
                projects.append(GLib.filename_to_uri(proj, None))

    if isinstance(options.paths, str):
        options.paths = [options.paths]

    fails = 0
    if options.wanted_tests:
        wanted_tests_patterns = []
        for pattern in options.wanted_tests.split(','):
            wanted_tests_patterns.append(re.compile(pattern))
    else:
        wanted_tests_patterns = None

    xunit = Xunit(options)
    keep_running = True
    while keep_running:
        if options.forever is False:
            keep_running = False

        for proj in projects:
            # First playback casses
            for scenario in SCENARIOS:
                classname = "playback.%s.%s" % (scenario, os.path.basename(proj))
                if not is_test_wanted(wanted_tests_patterns, classname):
                    continue

                if not launch_project(proj, classname, options, scenario=scenario,
                                      dest_uri=options.dest, xunit=xunit):
                    keep_running = False
                    if options.fatal_error:
                        print "%sFATAl error, quitting%s" % (Colors.FAIL, Colors.ENDC)
                        exit(report_final(xunit))

            # And now rendering casses
            for comb in COMBINATIONS:
                classname = "render.%s.%s" % (str(comb).replace(' ', '_'),
                                              os.path.basename(proj))
                if not is_test_wanted(wanted_tests_patterns, classname):
                    continue

                if not launch_project(proj, classname,
                                      options, combination=comb,
                                      dest_uri=options.dest,
                                      xunit=xunit):
                    keep_running = False
                    if options.fatal_error:
                        print "%sFATAl error, quitting%s" % (Colors.FAIL, Colors.ENDC)
                        exit(report_final(xunit))

    exit(report_final(xunit))
