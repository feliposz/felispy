# Felispy

A LISP-like interpreter written in C.

## Notes

This is my implementation of a LISP interpreter following the book [Build Your Own Lisp](https://www.buildyourownlisp.com/) by Daniel Holden.

I did it back in 2017. I kept separate folder for each chapter, but decided to recreate it as a git repository with a commit per chapter instead.

## Implementation

The interpreter is implemented in C and makes use of the [mpc (Micro Parser Combinators)](https://github.com/orangeduck/mpc) library by Daniel Holden, the same author of the book.

## Building

The project can be built in Visual Studio (tested on 2017 Community edition).

## Limitations

- Need to implement a standard library as suggest on the [last chapter](https://www.buildyourownlisp.com/chapter15_standard_library).
- Automated tests would be great
- Need examples/demos
