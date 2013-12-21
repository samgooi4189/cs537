import subprocess, os, Queue, sys, traceback, string
from time import time

class Failure(Exception):
   def __init__(self, value, detail=None):
      self.value = value
      self.detail = detail
   def __str__(self):
      if self.detail is not None:
         return str(self.value) + "\n" + str(self.detail)
      else:
         return str(self.value)

def addcslashes(s):
   special = {
      "\a":"\\a",
      "\r":"\\r",
      "\f":"\\f",
      "\n":"\\n",
      "\t":"\\t",
      "\v":"\\v",
      "\"":"\\\"",
      "\\":"\\\\"
      }
   r = ""
   for char in s:
      if char in special:
         r += special[char]
      elif char in string.printable:
         r += char
      else:
         r += "\\" + "{0:o}".format(ord(char))
   return r

def diff(expected, got):
   #length = min(expected, 30)
   #if len(got) > length*10:
   #   got = got[:length*5]
   s = ""
   s += "Expected: \"" + addcslashes(expected) + "\"\n"
   s += "Got:      \"" + addcslashes(got) + "\""
   return s

class Test:

   IN_PROGRESS = 1
   PASSED = 2
   FAILED = 3

   name = None
   description = None
   timeout = None

   def __init__(self, project_path = None, test_path = None, log = None,
         use_gdb = False, use_valgrind = False):
      self.project_path = project_path
      self.logfd = log
      self.state = Test.IN_PROGRESS
      self.notices = list()
      self.use_gdb = use_gdb
      self.use_valgrind = use_valgrind
      self.test_path = test_path

   def is_failed(self):
      return self.state == Test.FAILED

   def is_passed(self):
      return self.state == Test.PASSED

   def fail(self, reason = None):
      self.state = Test.FAILED
      if reason is not None:
         self.notices.append(reason)
      self.log("test " + self.name + " FAILED")

   def warn(self, reason):
      self.notices.append(reason)

   def done(self):
      self.logfd.flush()
      if not self.is_failed():
         self.state = Test.PASSED

   def __str__(self):
      s = "test " + self.name + " "
      if self.is_failed():
         s += "FAILED"
      elif self.is_passed():
         s += "PASSED"
      s += "\n"
      s += " (" + self.description + ")\n"
      for note in self.notices:
         s += " " + note + "\n"
      return s

   def runexe(self, name, args, libs = None, path = None,
         stdin = None, stdout = None, stderr = None, status = None):
      infd = None
      outfd = None
      errfd = None
      if stdin is not None:
         infd = subprocess.PIPE
      if stdout is not None:
         outfd = subprocess.PIPE
      if stderr is not None:
         errfd = subprocess.PIPE
      start = time()
      child = self.startexe(name, args, libs, path,
            stdin=infd, stdout=outfd, stderr=errfd)
      (outdata, errdata) = child.communicate(stdin)
      child.wallclock_time = time() - start
      if status is not None and status != child.returncode:
         raise Failure(str(name) + " returned incorrect status code. " +
               "Expected " + str(status) + " got " + str(child.returncode))
      if stdout is not None and stdout != outdata:
         raise Failure(str(name) + " gave incorrect standard output.",
               diff(stdout, outdata))
      if stderr is not None and stderr != errdata:
         raise Failure(str(name) + " gave incorrect standard error.",
               diff(stderr, errdata))
      return child



   def startexe(self, name, args, libs = None, path = None,
         stdin = None, stdout = None, stderr = None, cwd = None):
      if stdout is None:
         stdout = self.logfd
      if stderr is None:
         stderr = self.logfd
      if path is None:
         path = self.project_path + "/" + name
      if libs is not None:
         os.environ["LD_PRELOAD"] = libs
      if cwd is None:
         cwd = self.project_path
      args.insert(0, path)
      self.log(" ".join(args))
      print cwd
      if self.use_gdb:
         return subprocess.Popen(["xterm",
            "-title", "\"" + " ".join(args) + "\"",
            "-e", "gdb", "--args"] + args,
               cwd=cwd,
               stdin=stdin, stdout=stdout, stderr=stderr)
      if self.use_valgrind:
         return subprocess.Popen(["valgrind"] + args,
               stdin=stdin, stdout=stdout, stderr=stderr,
               cwd=cwd)
      else:
         return subprocess.Popen(args, cwd=cwd,
               stdin=stdin, stdout=stdout, stderr=stderr)
      if libs is not None:
         os.environ["LD_PRELOAD"] = ""
         del os.environ["LD_PRELOAD"]

   # run a utility program in project directory
   def run_util(self, args):
      self.log(" ".join(args))
      child = subprocess.Popen(args, cwd=self.project_path, stdout=self.logfd,
            stderr=self.logfd)
      status = child.wait()
      return status

   def after(self):
      pass

   def log(self, msg):
      self.logfd.write(msg + "\n")
      self.logfd.flush()


def run_test(test, queue):
   # create a new process group so we can easily kill all children of this proc
   os.setpgrp()
   try:
      test.run()
   except Failure as f:
      test.fail(str(f.value))
      (type, value, tb) = sys.exc_info()
      traceback.print_exception(type, value, tb, file=sys.stdout)
   except Exception as e:
      test.fail("Unexcpected exception " + str(e))
      (type, value, tb) = sys.exc_info()
      traceback.print_exception(type, value, tb, file=sys.stdout)
   finally:
      queue.put(test)

