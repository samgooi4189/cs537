from server import *
from test_list import *
import sys, os, shutil

TEST_PATH = "/u/c/s/cs537-2/public/test_scripts/p4"
args = sys.argv
if len(args) != 3:
    print "Usage: ./run <test_name> <port>"
    exit()

test_name = args[1]
port = int(args[2])
project_path = os.getcwd()
all_tests = test_list(project_path, TEST_PATH, sys.stdout)
log = sys.stdout

for f in ["home.html", "1.cgi", "2.cgi", "3.cgi", "output.cgi", "in"]:
    if not os.path.isfile(os.path.join(project_path, f)):
        shutil.copy(os.path.join(TEST_PATH, "files", f), project_path)
    

if test_name == "build":
    _test = build_test
else:
    match = None
    for test in all_tests:
        if test.name == test_name:
            match = test
            break
    if match is not None:
        _test = match
    else:
        sys.stderr.write(test_name + " is not a valid test\n")
        exit(2)

log.write("\n")
log.write("*" * 70 + "\n")
log.write("\n")
log.write("Test " + test.name + "\n")
log.write(test.description + "\n")
log.write("\n")
log.write("*" * 70 + "\n")
log.write(("Using server on port %d\n") % (port)) 
log.write("This does not rebuild your program\n")
log.write("Make sure your server is started with the same arguments as the test you want to run\n\n")
log.flush()

t = _test(log=log, project_path=project_path, test_path=TEST_PATH)
t.run(port)
if t.is_failed():
    print ("\nTest %s Failed") % (test_name)
else:
    print ("\nTest %s Passed") % (test_name)
