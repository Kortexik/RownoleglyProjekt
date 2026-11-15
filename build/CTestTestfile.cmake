# CMake generated Testfile for 
# Source directory: /Users/konradku/ProgramowanieRównoległeProjekt
# Build directory: /Users/konradku/ProgramowanieRównoległeProjekt/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(shared_mathematics "/Users/konradku/ProgramowanieRównoległeProjekt/build/mc_math_tests")
set_tests_properties(shared_mathematics PROPERTIES  _BACKTRACE_TRIPLES "/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;50;add_test;/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;0;")
add_test(numerical_openmp "/Users/konradku/ProgramowanieRównoległeProjekt/build/mc_tests")
set_tests_properties(numerical_openmp PROPERTIES  _BACKTRACE_TRIPLES "/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;53;add_test;/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;0;")
add_test(sequential_comparison "/Users/konradku/ProgramowanieRównoległeProjekt/build/mc_sequential_tests")
set_tests_properties(sequential_comparison PROPERTIES  _BACKTRACE_TRIPLES "/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;56;add_test;/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;0;")
add_test(program_output "/opt/homebrew/Frameworks/Python.framework/Versions/3.15/bin/python3.15" "/Users/konradku/ProgramowanieRównoległeProjekt/tests/test_program.py" "/Users/konradku/ProgramowanieRównoległeProjekt/build/mc_omp" "omp")
set_tests_properties(program_output PROPERTIES  _BACKTRACE_TRIPLES "/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;59;add_test;/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;0;")
add_test(sequential_output "/opt/homebrew/Frameworks/Python.framework/Versions/3.15/bin/python3.15" "/Users/konradku/ProgramowanieRównoległeProjekt/tests/test_program.py" "/Users/konradku/ProgramowanieRównoległeProjekt/build/mc_seq" "seq")
set_tests_properties(sequential_output PROPERTIES  _BACKTRACE_TRIPLES "/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;62;add_test;/Users/konradku/ProgramowanieRównoległeProjekt/CMakeLists.txt;0;")
