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

## To compile:

```sh
$ git clone https://github.com/rsachetto/odecompiler
$ cd odecompiler
$ make
```

## Using the interactive shell (needs gnuplot in path for the plot command):

```
$ ./bin/ode_shell
Current directory odecompiler
ode_shell> cd examples
ode_shell> load sir.ode
ode_shell> run
ode_shell> plot
```

![alt text](https://raw.githubusercontent.com/rsachetto/odecompiler/master/examples/imgs/sir_ode.png)

```
ode_shell> vars
Model vars for model sir:
t, 1
S, 2
I, 3
R, 4
ode_shell> setploty I
ode_shell> replot
```
![alt text](https://raw.githubusercontent.com/rsachetto/odecompiler/master/examples/imgs/sir_ode2.png)

```
ode_shell> setploty R
ode_shell> replot
ode_shell> exit
```
![alt text](https://raw.githubusercontent.com/rsachetto/odecompiler/master/examples/imgs/sir_ode3.png)


