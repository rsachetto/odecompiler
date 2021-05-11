# odecompiler
A simple compiler and an interactive shell to play with Ordinary Differential Equations (ODEs). 
The ODEs can be described using a very simple language and compiled to c source code (more languages will be available). It is also
possible to use the interactive shell to load, solve and plot the ODEs.

Below is the Lotka-Volterra model using the ode language:

```
alpha = 1
beta  = 1
delta = 1
gamma = 1

initial x = 2
initial y = 2

ode x' = x*(alpha - beta*y)
ode y' = y*(delta*x- gamma)
```
