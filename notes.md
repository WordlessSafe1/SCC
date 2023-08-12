# Notes

## Operator Precedence
| Precedence | Operator | Description                              | Associativity |
|------------|----------|------------------------------------------|---------------|
| 1          | `++` `--`| Postfix increment / decrement            | Left-to-right |
| 1          | `()`     | Function Call                            | Left-to-right |
| 1          | `[]`     | Array Subscripting                       | Left-to-right |
| 1          | `.`      | Struct/Union Member Access               | Left-to-right |
| 1          | `->`     | Dereferencing Struct/Union Member Access | Left-to-right |
|            |          |                                          |               |
| 2          | `++` `--`| Prefix increment / decrement             | Right-to-left |
| 2          | `-`      | Negation                                 | Right-to-left |
| 2          | `!` `~`  | Logical NOT / bitwise NOT                | Right-to-left |

<!-- `=||` will have precedence directly below ==/!= -->

## Quirks:
_Currently, no notable quirks._

## Added Features:
- **Elivs Operator** - The Ternary Operator (`<conditon> ? <true> : <false>`) can be called without a middle operand to implicitly pass the result of the condition. The form is: `<condition> ?: <false>`.  
	Example:
	```c
	int a = 3 ?: 2; // a is 3
	```
- **Repeated Comparison Operators** - A new type of operator has been added, _Repeated Comparison Operators_. They will evaluate a comparison of the lefthand side against multiple right hand expressions.  
	Currently, only the [**Repeated Short-Circuting Logical Equality Operator**](#repeated-short-circuiting-logical-equality-operator) is implemented. 


### Repeated Short-Circuiting Logical Equality Operator:
The repeated short-circuiting logical equality operator (henceforth referred to by its symbol, `=||`) evaluates the left hand side of the expression once, and compares it against each of the expressions in the expression list on the right hand side until it reaches either a matching expression or the end of the list. It will return zero if no match is found, or one if a match is found.  

The high-level behaviour of the following two snippets are identical:
```c
int GetTwo() { return 2; }
void main(){
	int b;
	{
		int a = GetTwo();
		// GetTwo() is called once
		b = a == 1 || a == 2 || a == GetThree();
		// b is 1
		// GetThree() is never called 
	}
}
```
```c
int GetTwo() { return 2; }
void main(){
	int b = GetTwo() =|| (1, 2, GetThree());
	// GetTwo() is called once
	// b is 1
	// GetThree() is never called
}
```
However, the latter snippet has baked-in optimizations.