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

alex = Shared[Hello]("Hi")  # Shared ownership example
steve = alex   # Now they are sharing the same object
alex.greet()   # Works
steve.greet()  # Also works

config = ConstShared[Config]("config.json")  # ConstShared for immutable shared ownership
# config.value = "new_config.json"  # Error: ConstShared cannot be modified
x = config # Ownership is shared, but cannot modify the value
print(config.file)
print(x.file)  # Both print "config.json"
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
