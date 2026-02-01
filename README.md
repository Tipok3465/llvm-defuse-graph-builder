# llvm def-use graph builder

this tool builds a graph from llvm ir:
- cfg edges (control flow)
- def-use edges (data dependencies)
- optional runtime values on nodes (from a log)

it can output:
- `enhanced_graph.dot`
- `enhanced_graph.png`
- `enhanced_graph.svg`

## requirements

- clang
- llvm `opt`
- graphviz `dot` (for png/svg)

quick check:
```bash
which clang opt dot
````

## build

```bash
./build.sh
```

binary:

```bash
./bin/defuse-analyzer --help
```

## run tests

```bash
./run_simple.sh
./run_medium.sh
./run_complex.sh
```

results are saved in `outputs/`.

## analyze your own file (one command)

```bash
./bin/defuse-analyzer -analyze path/to/main.c
```

this runs the full pipeline:

1. clang -> `.ll`
2. mem2reg
3. instrument
4. run instrumented program -> `runtime.log`
5. build graph -> `enhanced_graph.*`

output folder:

* `outputs/<file_name>/`

## step-by-step commands

c -> ll:

```bash
./bin/defuse-analyzer -emit-llvm file.c out.ll
```

mem2reg:

```bash
./bin/defuse-analyzer -mem2reg in.ll out.ll
```

instrument:

```bash
./bin/defuse-analyzer -instrument in.ll out.ll
```

build + run (creates runtime.log):

```bash
./bin/defuse-analyzer -run instrumented.ll outputs/runtime.log outputs/program
```

graph:

```bash
./bin/defuse-analyzer -graph in.ll outputs/runtime.log outputs/enhanced_graph.dot
```


--- 

