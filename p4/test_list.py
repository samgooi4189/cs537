tester_files = ["server"]

def test_list(project_path, test_path, log):
   l = list()
   for f in tester_files:
      module = __import__(f)
      l.extend(module.test_list)
   return l

from build import BuildTest
build_test = BuildTest
build_test.targets = ["server"]
