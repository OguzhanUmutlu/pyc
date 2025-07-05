# PYC - An unfinished business

Hello, there! This was an attempt to make a language that has the same features and syntax as Python, but is compiled to
C. I made the lexer and parser. Started designing the compiler, but realized a crucial problem.

# About Lexer and Parser

They are pretty easy to use, it includes every feature of Python 3.10. Has been tested in 11k lines of sympy source
code. Here are the definitions:

```c++
// src/lexer.h
void pyc_lexer_init(char *code)
void pyc_tokenize()
```

```c++
// src/parser.h
node *parse_file(node *parent, char *filename, char *code)
```

**This implementation was optimized for C so it is about 4-5 times faster than the official Python lexer and parser.**

*A lot of micro-optimizations combined~*

That's it! You can see what happened in the writing of `compiler.c` in the rest of this document.

# About the Compiler

I first modeled two base conversions. The following conversions are simplified, normally Python integers can be of
arbitrary
size, but I'm using `long` to simplify the look on the examples.

## 1) If-else statements

When dynamically typed variables are given into an if-else statement, I had two potential paths to take.

We will be compiling the following code:

```py
a = 10

print(a)

a = input()

print(a)

if len(a) > 5:
    a = 5.7
    a = int(a)
else:
    a = float(a)

print(a)
```

## 1.a) Compile with a tagged union

The compiler creates a tag for the variable and uses it to determine the type of the variable inside the if-else
statements. Then after the if-else statement ends, it is used as a tagged variable.

```c++
#include <pyc_stdlib.h>
#include <stdio.h>

int main() {
    union{long i; double f; string s;} a;
    a.i = 10;
    printf("%ld\n", a.i);
    a.s = input("");
    printf("%s\n", a.s.data);
    char a_tag = 0; // 0 for int, 1 for float
    if (a.s.length > 5) {
        a.f = 5.7;
        // We don't have to change the tag here, tag is only set at the end.
        a.i = string2int(a.s);
        a_tag = 0; // int
    } else {
        a.f = string2float(a.s);
        a_tag = 1; // float
    }
    
    if (a_tag == 0) {
        printf("%ld\n", a.f);
    } else if (a_tag == 1) {
        printf("%f\n", a.i);
    }
}
```

This is computationally expensive, because the compiler has to check the type of the variable every time it is used.
This is not a problem for small programs, but for larger programs, it can become a bottleneck and whatever we do this
will be equally fast as Python as slower than Python JITs like PyPy which do runtime analysis. Second approach is much
faster, but it has a problem of its own too.

## 1.b) Compile by branching

The compiler creates a tag for the variable and uses it to determine the type of the variable inside the if-else
statements. Then after the if-else statement ends, it is used as a tagged variable.

```c++
#include <pyc_stdlib.h>
#include <stdio.h>

int main() {
    union{long i; double f; string s;} a;
    a.i = 10;
    printf("%ld\n", a.i);
    a.s = input("");
    printf("%s\n", a.s.data);
    // variable a gets in a conditional and splits and accumulates the rest of the code. this is one way of doing this.
    if (a.s.length > 5) {
        a.i = string2int(a.s);
        printf("%ld\n", a.i);
    } else {
        a.f = string2float(a.s);
        printf("%f\n", a.f);
    }
}
```

The problem with this approach is that the compiler has to generate a lot of code for each if-else statement, and it
grows exponentially with the number of if-else statements. This is a huge problem if there are too many dynamically
typed variables. And as far as I could see, there is no practical way to solve this problem of having dynamically typed
variables get in branches or loops as we will see in a bit.

## 2) Loops

Problem: Loops can get in variables from outside and change it's type to something unknown depending on the loop's
behavior (in this case it's the iteration count). In this scenario, the only solution I could find is to make the
variable a tagged union and use the same approach as in the if-else statements.

Applying that logic to the following code:

```py
a = 10

for i in range(100):
    if i % 2 == 0:
        a = str(i)
    else:
        a = i

print(a)
```

Is compiled to:

```c++
#include <pyc_stdlib.h>
#include <stdio.h>

int main() {
    union{long i; string s} a;
    // We split now because the loop will be changing the type of a.
    char a_t = 0; // 0 for int, 1 for string
    for (union{long i} i = 0; i < 100; i++) {
        if (i.i % 2 == 0) {
            a.i = i.i;
            // do this at the end of the conditional
            a_t = 0;
        } else {
            a.s = int2string(i.i);
            // do this at the end of the conditional
            a_t = 1;
        }
    }
    if (a_t == 0) {
        printf("%ld\n", a.i);
    } else {
        printf("%s\n", a.s);
    }
    return 0;
}
```

This brings a new problem, we are doing the same thing as the first approach of if-else statements, but now we can't
even branch if we wanted to. And this again is computationally expensive, and has no use, and is potentially even slower
than Python.

# Conclusion

In the compiled codes above I notated the tag as with two types (for example `int` or `float`), the problem is that the
amount of types a variable has can get into thousands! So this is not even close to a practical solution. The only
sensible way seems to be to just use a general type struct like `PyObject`. So until you or I get a new idea that can
solve any two of these problems, this concludes a month-long journey of me trying to make a Python-like language that
compiles to C. Was good practice of C though I have to say, after all I did write an entire lexer and parser in C, which
was a lot of fun. Thanks for reading!
