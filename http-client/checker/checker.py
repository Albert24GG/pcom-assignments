#!/usr/bin/env python3
"""
Checker helper for PCom HTTP client assignment.

Dependencies: pexpect, requests (see requirements.txt)
"""

import os
import sys
import string
import random
import hashlib
import time
import shlex
import traceback
from collections import namedtuple
import argparse
import re
import textwrap

import pexpect
import yaml


EXPECT_TIMEOUT = 1  # 1 second should be enough...
EXPECT_SEP = "="  # field separator for input
TEXT_INDENT = "    "

# Regex for parsing ERROR/SUCCESS command statuses
RE_SUCCESS = re.compile(r"^.*((?:^|\W)succ?ess?).*$", re.IGNORECASE)
RE_ERROR = re.compile(r"^.*((?:^|\W)err?oa?r|(?:^|\W)fail).*$", re.IGNORECASE)
# Regexes for extracting useful information
RE_EXTRACT_LIST_ITEM = r"^\s*(?:movie\s*)?#([0-9]+)(?:\s*:)?\s*(.+)\s*"
RE_EXTRACT_OBJ_FIELD = r"^\s*%s[ \t\f\v]*[=:][ \t\f\v]*([^\r\n]+)\s*?|\"%s\"\s*:\s*(?:\"([^\"]+)|([0-9]+))"

# tests object; loaded at runtime
T = {"scripts": {}}


class CheckerException(Exception):
    """ Class of checker-thrown exceptions. """
    pass

def wrap_test_output(text, indent=TEXT_INDENT):
    return textwrap.indent(text, prefix=indent)

def color_print(text, fg="white", bg=None, style="normal", stderr=False, newline=True):
    """ Colored ANSI terminal printing
        (forgive this mess... but it's better than extra dependencies ;) """
    COLORS = ["black", "red", "green", "yellow", "blue", "purple", "cyan", "white"]
    STYLES = ["normal", "bold", "light", "italic", "underline", "blink"]
    fg = str(30 + COLORS.index(fg.lower()))
    bg = ("" if not bg else ";" + str(40 + COLORS.index(bg.lower())))
    style = str(STYLES.index(style.lower()))
    text = '\033[{style};{fg}{bg}m{text}\033[0;0m'.format(text=text, fg=fg, bg=bg, style=style)
    if newline:
        text += "\n"
    (sys.stderr if stderr else sys.stdout).write(text)

class ExpectInputWrapper:
    """ Utility IO class used for prefixing the debug output. """
    def __init__(self, direction=False):
        self._dir = direction
    def write(self, s):
        prefix = "  > " if self._dir else "  < "
        color_print(wrap_test_output(s.strip(), indent=prefix),
                    fg="white", style="italic")
    def flush(self): pass

def normalize_user(user_type, user_data, randomize=False):
    if user_data and isinstance(user_data, str):
        user_pair = user_data.split(":")
        return { "username": user_pair[0], "password": user_pair[1] }
    if randomize:
        alphanum = string.ascii_letters + string.digits
        rand_user = f"test_{int(time.time())}_" \
            f"{''.join(random.choices(alphanum, k=6))}"
        rand_password = hashlib.sha256(os.urandom(16)).hexdigest()[:20]
        return { "username": rand_user, "password": rand_password }
    raise CheckerException(f"No {user_type} provided!")

def expect_send_params(p, xvars):
    keys = list(xvars.keys())
    xpatterns =  [(r"(?i)\b(" + kw + r")\s*" + EXPECT_SEP + r"\s*") for kw in keys]
    xseen = set()
    while xseen != set(keys):
        try:
            idx = p.expect(xpatterns)
            p.sendline(str(xvars.get(keys[idx])))
            xseen.add(keys[idx])
        except pexpect.exceptions.TIMEOUT:
            raise CheckerException("Client did not ask the following fields: " + 
                                   ", ".join(set(keys) - xseen))

def expect_flush_output(p, ignore_error=False):
    i = p.expect([pexpect.TIMEOUT, pexpect.EOF, RE_ERROR])
    buf = p.before
    if i == 0 and p.before:
        p.expect(r'.+')
    if i == 2 and not ignore_error:
        raise CheckerException(f"Program returned error: {p.after.strip()}")
    return buf

def expect_print_output(p, ignore_error=False):
    res = p.expect([RE_SUCCESS, RE_ERROR, pexpect.TIMEOUT])
    color_args = {}
    text = p.after
    if res == 0:
        color_args = {"fg": "green"}
    elif res == 1:
        if not ignore_error:
            raise CheckerException(f"Program returned error: {text.strip()}")
        color_args = {"fg": "red"}
    else:
        text = p.before or "<no output>"
        if p.before:
            p.expect(r'.+')
    color_print(wrap_test_output(text.strip()), **color_args)

def extract_list_items(output):
    matches = re.findall(RE_EXTRACT_LIST_ITEM, output, re.I | re.M)
    return [(val[0].strip(), val[1].strip()) for val in matches]

def extract_object_field_vals(output, field):
    matches = re.findall(RE_EXTRACT_OBJ_FIELD % (field, field), output, re.I | re.M)
    return [val[0] or val[1] or val[2] for val in matches]

def extract_object_fields(output, fields):
    obj = {}
    for field in fields:
        vals = extract_object_field_vals(output, field)
        if not vals:
            raise CheckerException("Cannot find obj field '%s'" % field)
        if len(vals) != 1:
            raise CheckerException("Multiple '%s' fields found in output: %s!" % (field, vals))
        obj[field] = vals[0]
    return obj

def get_object_id_by_idx(name, xargs, idx=None):
    objs = xargs.get(f"{name}_objs")
    if idx is None:
        idx = xargs.get(f"{name}_idx")
    if not objs:
        raise CheckerException(f"No {name} objects found!")
    if idx >= len(objs):
        raise CheckerException(f"No {name} with index #{idx}")
    return objs[idx][0]

def check_object_fields(obj, expected):
    fields = expected.keys()
    for field in fields:
        val = obj.get(field)
        if str(val) != str(expected[field]):
            raise CheckerException("Field '%s' mismatch: %s != %s" % 
                (field, val, expected[field]))

def do_login_admin(p, xargs):
    p.sendline("login_admin")
    expect_send_params(p, xargs["admin_user"])
    expect_print_output(p)

def do_add_user(p, xargs):
    p.sendline("add_user")
    expect_send_params(p, {
        "username": xargs["normal_user"]["username"], 
        "password": xargs["normal_user"]["password"]
    })
    expect_print_output(p)

def do_delete_user(p, xargs):
    p.sendline("delete_user")
    expect_send_params(p, {
        "username": xargs["normal_user"]["username"], 
    })
    expect_print_output(p)

def do_get_users(p, xargs):
    p.sendline("get_users")
    buf = expect_flush_output(p)
    xargs["user_objs"] = extract_list_items(buf)
    if xargs.get("debug"):
        color_print(wrap_test_output("Extracted users: \n" + str(xargs["user_objs"])), fg="white")
    user_found = False
    for obj in xargs["user_objs"]:
        user_params = obj[1].split(":")
        if user_params[0] == xargs["normal_user"]["username"]:
            user_found = True
    xargs["_user_ensured"] = user_found

def do_delete_all_users(p, xargs):
    objs = xargs.get("user_objs")
    if not objs and not xargs.get("delete_ignore", False):
        raise CheckerException("No users found!")
    for obj in objs:
        user_params = obj[1].split(":")
        if (not xargs.get("delete_pattern") or
                re.match(xargs.get("delete_pattern"), user_params[0])):
            do_delete_user(p, { "normal_user": { "username": user_params[0] } })

def do_logout_admin(p, xargs):
    p.sendline("logout_admin")
    expect_print_output(p)

def do_login(p, xargs):
    p.sendline("login")
    expect_send_params(p, xargs["normal_user"])
    expect_print_output(p)

def do_logout(p, xargs):
    p.sendline("logout")
    expect_print_output(p)

def do_get_access(p, xargs):
    p.sendline("get_access")
    expect_print_output(p)

def ensure_user_access(p, xargs):
    ensure_script = [ ["login", {}], ["get_access", {}] ]
    if not xargs.get("_user_ensured", False):
        ensure_script = [
            ["login_admin", {}], ["add_user", {}],
            ["get_users", {}], ["logout_admin", {}],
        ] + ensure_script
    else:
        print(wrap_test_output("User already ensured!"))
    nxargs = dict(xargs, script=ensure_script, dont_exit=True)
    run_tasks(p, nxargs)

def do_get_movies(p, xargs):
    p.sendline("get_movies")
    buf = expect_flush_output(p)
    xargs["movie_objs"] = extract_list_items(buf)
    if xargs.get("debug"):
        color_print(wrap_test_output("Extracted movies: \n" + str(xargs["movie_objs"])), fg="white")
    expect_count = xargs.get("expect_count", False)
    if type(expect_count) is int:
        if len(xargs["movie_objs"]) != expect_count:
            raise CheckerException("Movies count mismatch: %s != %s" % 
                                   (len(xargs["movie_objs"]), expect_count))
        color_print(wrap_test_output("PASSED: count=%i" % expect_count), fg="green", style="bold")

def do_add_movie(p, xargs):
    existing_titles = [obj[1] for obj in xargs.get("movie_objs", [])]
    movie_obj = xargs.get("movie_obj", {})
    if movie_obj["title"] in existing_titles:
        color_print(wrap_test_output("SKIP: object already exists!"), fg="yellow")
        return
    p.sendline("add_movie")
    movie_struct = { key : movie_obj.get(key, "")
        for key in ("title", "year", "description", "rating") }
    expect_send_params(p, movie_struct)
    expect_print_output(p)

def do_delete_movie(p, xargs):
    movie_id = get_object_id_by_idx("movie", xargs)
    p.sendline("delete_movie")
    expect_send_params(p, {"id": movie_id})
    expect_print_output(p)

def do_get_movie(p, xargs):
    movie_id = get_object_id_by_idx("movie", xargs)
    p.sendline("get_movie")
    expect_send_params(p, {"id": movie_id})
    buf = expect_flush_output(p)
    obj = extract_object_fields(buf, ("title", "description", "year", "rating"))
    if xargs.get("debug"):
        color_print(wrap_test_output("Extracted object: %s" % obj), fg="white")
    expected = xargs.get("expect_movie", False)
    if expected:
        check_object_fields(obj, expected)
        color_print(wrap_test_output("PASSED: fields match!"), fg="green", style="bold")

def do_delete_all_movies(p, xargs):
    objs = xargs.get("movie_objs")
    if not objs and not xargs.get("delete_ignore", False):
        raise CheckerException("No movies found!")
    for obj in objs:
        p.sendline("delete_movie")
        expect_send_params(p, {"id": obj[0]})
        expect_print_output(p)

def do_get_collections(p, xargs):
    p.sendline("get_collections")
    buf = expect_flush_output(p)
    xargs["collection_objs"] = extract_list_items(buf)
    if xargs.get("debug"):
        color_print(wrap_test_output("Extracted collections: \n" + str(xargs["collection_objs"])), fg="white")
    expect_count = xargs.get("expect_count", False)
    if type(expect_count) is int:
        if len(xargs["collection_objs"]) != expect_count:
            raise CheckerException("Collections count mismatch: %s != %s" % 
                                   (len(xargs["collection_objs"]), expect_count))
        color_print(wrap_test_output("PASSED: count=%i" % expect_count), fg="green", style="bold")
    expect_titles = xargs.get("expect_title", None)
    if expect_titles:
        existing_titles = [obj[1] for obj in xargs.get("collection_objs", [])]
        for exp_title in expect_titles:
            if exp_title not in existing_titles:
                raise CheckerException(f"Collection not found: '{exp_title}'")

def do_add_collection(p, xargs):
    existing_titles = [obj[1] for obj in xargs.get("collection_objs", [])]
    collection_obj = xargs.get("collection_obj", {})
    if collection_obj["title"] in existing_titles:
        color_print(wrap_test_output("SKIP: object already exists!"), fg="yellow")
        return
    p.sendline("add_collection")
    collection_struct = {
        "title": collection_obj.get("title", ""),
        "num_movies": len(collection_obj.get("movie_idx", [])),
    }
    movies_ids = [get_object_id_by_idx("movie", xargs, idx=idx)
        for idx in collection_obj.get("movie_idx", [])]
    for idx, movie_id in enumerate(movies_ids):
        collection_struct[r"movie_id\[\s*" + str(idx) + r"\s*\]"] = movie_id
    expect_send_params(p, collection_struct)
    expect_print_output(p)

def do_get_collection(p, xargs):
    collection_id = get_object_id_by_idx("collection", xargs)
    p.sendline("get_collection")
    expect_send_params(p, {"id": collection_id})
    buf = expect_flush_output(p)
    obj = extract_object_fields(buf, ("title", "owner"))
    obj["movies"] = extract_list_items(buf)
    if xargs.get("debug"):
        color_print(wrap_test_output("Extracted object: %s" % obj), fg="white")
    expected_fields = xargs.get("expect_collection", False)
    if expected_fields:
        check_object_fields(obj, expected_fields)
        color_print(wrap_test_output("PASSED: fields match!"), fg="green", style="bold")
    expected_movies = xargs.get("expect_movies", False)
    if expected_movies:
        movie_titles = [x[1] for x in obj["movies"]]
        if (set(movie_titles) != set(expected_movies)):
            raise CheckerException(f"Collection movies mismatch: {movie_titles} != {expected_movies}!")
        color_print(wrap_test_output("PASSED: movies match!"), fg="green", style="bold")

def do_delete_collection(p, xargs):
    collection_id = get_object_id_by_idx("collection", xargs)
    p.sendline("delete_collection")
    expect_send_params(p, {"id": collection_id})
    expect_print_output(p)

def do_exit(p, xargs):
    if xargs.get("dont_exit", False):
        return
    p.sendline("exit")
    p.expect(pexpect.EOF)

def interactive_shell(p, xargs):
    p.setecho(True)
    p.interact()


ACTIONS = {
    "login_admin": do_login_admin,
    "add_user": do_add_user, "get_users": do_get_users,
    "delete_all_users": do_delete_all_users,
    "login": do_login, "get_access": do_get_access,
    "ensure_user_access": ensure_user_access,
    "get_movies": do_get_movies, "get_movie": do_get_movie, "add_movie": do_add_movie,
    "delete_movie": do_delete_movie, "delete_all_movies": do_delete_all_movies,
    "add_collection": do_add_collection, "get_collections": do_get_collections,
    "get_collection": do_get_collection, "delete_collection": do_delete_collection,
    "logout_admin": do_logout_admin, "logout": do_logout,
    "exit": do_exit, "shell": interactive_shell,
}

def load_tests(tests_file):
    global T
    with open(tests_file, "r") as f:
        T = yaml.safe_load(f)

def run_tasks(p, args):
    PROPAGATE_FIELDS = ["_user_ensured"]
    script_name = args.get("script")
    if not script_name:
        script_name = "full"
    if not isinstance(script_name, list):
        if script_name not in T["scripts"]:
            raise CheckerException("Invalid script: %s" % (script_name))
        script = T["scripts"][script_name]
    else:
        script = script_name
    parent_xargs = args
    xargs = parent_xargs
    for task in script:
        ignore = xargs.get("ignore", False)
        action = None
        action_name = ""
        print_style = {"fg": "cyan", "style": "bold"}
        if isinstance(task, (tuple, list)):
            action_name = task[0]
            action = ACTIONS[action_name]
            if task[1]:
                xargs.update(task[1])
            ignore = xargs.get("ignore", False)
        elif isinstance(task, str):
            action_name = "<RUN SUBTASK: %s>" % str(task)
            print_style = {"fg": "black", "bg": "purple", "style": "bold"}
            action = run_tasks
            xargs = dict(parent_xargs, script=task, dont_exit=True)
        if not action:
            continue
        try:
            color_print("%s: " % action_name, **print_style)
            action(p, xargs)
            for field in PROPAGATE_FIELDS:
                if field in xargs:
                    parent_xargs[field] = xargs[field]
        except CheckerException as ex:
            ex = CheckerException("%s: %s" % (action.__name__, str(ex)))
            color_print(wrap_test_output("ERROR:"), fg="black", bg="red", stderr=True, newline=False)
            color_print(wrap_test_output(str(ex)), fg="red", stderr=True)
            if parent_xargs.get("debug"):
                color_print(wrap_test_output(traceback.format_exc()), fg="red", stderr=True)
            if not ignore:
                break
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='checker.py',
                                     description='Python helper for PCom / HTTP Assignment')
    parser.add_argument('program')
    parser.add_argument('--test-file', help="Specify a custom tests yaml file",
                        default="sample-tests.yaml")
    parser.add_argument('--script', default="full")
    parser.add_argument('-a', '--admin', dest="admin_user", help="Specify admin username & password " \
        "(separated by colon, e.g. `-a admin_user:password`)")
    parser.add_argument('-u', '--user', dest="normal_user", help="Override normal username & password " \
        "(separated by colon, e.g. `-u normal_username:password`; default: random)")
    parser.add_argument('-d', '--debug', help="Enable debug output", action="store_true")
    parser.add_argument('-i', '--ignore', help="Ignore errors (do not exit test early)", action="store_true")

    args = parser.parse_args()
    p = pexpect.spawn(shlex.quote(args.program), encoding='utf-8', echo=False, timeout=EXPECT_TIMEOUT)
    if args.debug:
        p.logfile_send = ExpectInputWrapper(direction=True)
        p.logfile_read = ExpectInputWrapper(direction=False)

    try:
        load_tests(args.test_file)
        xargs = vars(args)
        xargs["admin_user"] = normalize_user("admin_user", xargs.get("admin_user"))
        xargs["normal_user"] = normalize_user("normal_user", xargs.get("normal_user"), True)
        xargs["normal_user"]["admin_username"] = xargs["admin_user"]["username"]
        if args.debug:
            print("xargs: ", str(xargs))
        run_tasks(p, xargs)
    except Exception as ex:
        color_print("FATAL ERROR:", fg="black", bg="red", stderr=True, newline=False)
        color_print(" " + str(ex), fg="red", stderr=True)
        if args.debug:
            color_print(traceback.format_exc(), fg="red", stderr=True)

