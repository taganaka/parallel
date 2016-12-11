# parallel

Damn simple program for executing jobs in parallel on a local system

It takes only one argument, a file containing a list of commands. Each command will be executed in parallel.

That's it, no bullshit

## Compile & Install

```bash
$ make install
```

## Usage
```
$ parallel cmds.txt
```

Where cmds.txt is a text file containing on each line a command that will be executed in parallel.
