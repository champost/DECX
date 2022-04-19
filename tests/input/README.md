Use scripts in this folder to check that input files are parsed correctly,
and that they produce adequate error messages.
To this end, DECX is called with hidden dry runs arguments,
and its output on stdout and stderr are just compared to expectations.

Most the folder contains machinery to run the tests.  
Actual tests specifications stand in `specs/`.  
Toy configuration files stand in `dummy_files/`.

Run the tests with `./main.py`.
