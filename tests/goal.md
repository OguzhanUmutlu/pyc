# Types

There are a few new types in addition to the standard Python types.

- `uint8` - unsigned 8-bit integer (0 to 255)
- `uint16` - unsigned 16-bit integer (0 to 65535)
- `uint32` - unsigned 32-bit integer (0 to 4294967295)
- `uint64` - unsigned 64-bit integer (0 to 18446744073709551615)
- `int8` - signed 8-bit integer (-128 to 127)
- `int16` - signed 16-bit integer (-32768 to 32767)
- `int32` - signed 32-bit integer (-2147483648 to 2147483647)
- `int64` - signed 64-bit integer (-9223372036854775808 to 9223372036854775807)
- `float32` - 32-bit floating point number (single precision)
- `float64` - 64-bit floating point number (double precision)
- `uint` - This is an alias for `uint64` (or `uint32` on 32-bit systems). Also called as word size unsigned integer.
- `int` - This is an alias for `int64` (or `int32` on 32-bit systems). Also called as word size integer.
- `float` - Alias for `float64`.
- `bool` - Can only be `True` or `False`, extends `uint8`.
- `str` - Represents a string, extends `list[uint8]`.
- ``

# Running Python code

You can use the `python` module to run Python code directly.

If you directly use it in variables, it will be compiled to bytecode at compile time and executed at runtime.

```py
from python import numpy

matrix_a = numpy.array([[1, 2, 3, 4],
                        [5, 6, 7, 8],
                        [9, 10, 11, 12],
                        [13, 14, 15, 16]])
identity_matrix = numpy.array([[1, 0, 0, 0],
                               [0, 1, 0, 0],
                               [0, 0, 1, 0],
                               [0, 0, 0, 1]])
result = numpy.matmul(matrix_a, identity_matrix)
```

Or you can use `comp` variables which directly get interpreted at compile time, not affecting the output.

```py
from python import numpy

comp matrix_a = numpy.array([[1, 2, 3, 4],
                             [5, 6, 7, 8],
                             [9, 10, 11, 12],
                             [13, 14, 15, 16]])
comp identity_matrix = numpy.array([[1, 0, 0, 0],
                                    [0, 1, 0, 0],
                                    [0, 0, 1, 0],
                                    [0, 0, 0, 1]])
comp result = numpy.matmul(matrix_a, identity_matrix)
```

# Compile-time variables

These will not affect the runtime behavior of your code, but they can be used to optimize performance by pre-computing
values. They are also scoped variables so they are unreachable outside their scope.

```py
comp base_value = 10  # Executed once, even inside conditionals or loops
comp computed_value = base_value + 5 * 2

if False:
    comp x = 20  # This won't be executed, but 'x' is defined

print(x)  # Since it's a scoped variable, x doesn't exist here, will raise an error
```

# Function parameters and references

```py
def test(ref value):
    print(value)
    value = 10
    copied_value = value  # This is a copy, not a reference (unless 'value' is shared)
    return copied_value

input_value = 10
test(input_value)  # Prints 10 (original passed value)
print(input_value)  # Prints 10 (unchanged outside function)
test(10)  # This doesn't work as 10 is not an lvalue, it cannot be passed by reference
```

# Classes and template variables

Classes are defined same as in Python, but you can use template variables to hold types, allowing for type-safe generic
programming.

```py
class ValueHolder[T]:
    def __init__(self, value: T):
        self.value = value

    def greet(self):
        print(f"Hello, {self.value}!")


value = ValueHolder("World")  # Type inferred automatically as ValueHolder[str]
value.greet()                 # Prints "Hello, World!"

life = ValueHolder[int](42)  # Explicit type annotation
life.greet()                 # Prints "Hello, 42!"
```

# Template variables in functions

```py
def print_list[T](items: list[T]):
    for item in items:
        print(item)


print_list([1, 2, 3])
print_list(["a", "b", "c"])  # Works with different types
```

# Ownership

There are two types of ownership: `Box` and `Shared`.

When you create a `Box` it means when you assign it to another variable, the ownership is transferred and the original
variable becomes invalidated. This is similar to Rust's ownership model.

On the other hand, `Shared` allows multiple variables to own the same object, similar to reference counting in Python.

These are used to allocate memory on the heap while maintaining memory safety and ownership semantics.

```py
# Heap allocation examples
my_box = Box(ValueHolder("hi"))           # Using type inference
my_box_explicit = Box[ValueHolder]("hi")  # Explicitly specifying type, same effect

new_owner = my_box        # Ownership transferred to 'owner'
# heap_hello.greet()      # Error: original owner invalidated

alex = Shared[ValueHolder]("Hi")  # Shared ownership example
steve = alex   # Now they are sharing the same object
alex.greet()   # Works
steve.greet()  # Also works
```

# Scoped variables

Scoped variables are defined using the `var` keyword. They are similar to local variables in Python, but they are
scoped to the block they are defined in. They cannot be accessed outside their scope, which helps prevent accidental
modifications and makes the code cleaner.

```py
var x = 10
if x > 5:
    x = 30 # Sets the `x` outside this scope
    var x = 20
    x = 40  # This modifies the `x` in the outer scope
    print(x)  # Prints 40

print(x)  # Prints 30, the outer `x` was modified
```

# Constant variables

You can define constant variables using the `val` keyword. These are scoped variables that cannot be accessed outside
their scope and cannot be modified after they are defined. They are useful for defining values that should not change
throughout the program, similar to constants in other languages.

```py
val PI = 3.14159  # This is a constant, cannot be changed
val alex = Shared[ValueHolder]("hi")
# alex.value = "hello"  # Error: Cannot modify a constant variable
```

# Copy operator overloading

You can overload operators like `=`, `+=`, `-=`, etc., to define custom behavior for your classes.

```py
class CustomValue:
    def __init__(self, value: int):
        self.value = value
    
    def __iset__(self, other: int):
        self.value = other
    
    def __iadd__(self, other: int):
        self.value += other
    
    def __isub__(self, other: int):
        self.value -= other

my_value = CustomValue(10)
my_value += 5  # Calls __iadd__
print(my_value)  # Prints CustomValue(value=15)
my_value = 10 # Calls __iset__
print(my_value)  # Prints CustomValue(value=10)
my_value -= 3  # Calls __isub__
print(my_value)  # Prints CustomValue(value=7)
```

You can also define the `__repr__` method to control how your class is printed. By default it will print the class name
and its attributes. (If there is no `__repr__` it will use `__str__`.)
