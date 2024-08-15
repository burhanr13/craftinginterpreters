#!/bin/python

def fib(n):
    return n if n < 2 else fib(n-1)+fib(n-2)

sum = 0
for i in range(35):
    sum += fib(i)
print(sum)
