# CS422 Computer Architecture

Homework 1 submission of the semester 2023-24-I of CS422 at IIT Kanpur.

## Tools used:

- Intel pin-3.28-98749

## Codebase

- HW1.cpp : The main pin tool code for the assignment.
- Makefile : The makefile from pin examples to build the tool.
- HW1.txt : Problem statement of the assignment.
- [report.pdf](./report.pdf) : The report for the assignment.
- `runs/` : Contains the output of the tool for the given SPEC2006 benchmarks.

## Author

- Name: Harshit Raj
- Email: harshitr20@iitk.ac.in
- Roll: 200433
- Date: 26 Spet 2023
- Instructor: [Prof. Mainak Chaudhuri](https://www.cse.iitk.ac.in/users/mainakc/)

## Usage

```sh
# Build the tool
make TARGET=ia32 obj-ia32/HW1.so

# Run the tool on a benchmark
cd /path/to/spec_2006/400.perlbench/
pin -t /path/to/obj-ia32/HW1.so -f 207 -o perlbench.diffmail.out -- ./perlbench_base.i386 -I./lib diffmail.pl 4 800 10 17 19 300 > perlbench.ref.diffmail.out 2> perlbench.ref.diffmail.err
```

- `-f` flag is used to specify the fast-forward instruction count in billions.
- `-o` flag is used to specify the output file.
- `-t` flag is used to specify the pin tool to be used.
- `--` is used to separate the pin tool arguments from the application arguments.
