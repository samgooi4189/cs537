#!/bin/bash
base=/u/c/s/cs537-2/public/test_scripts
python=python
$python $base/p2/run-tests.py --project p2 --project_path "`pwd`" $@ | tee -i runtests.log
exit $?
