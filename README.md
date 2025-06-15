# FlowNet DSL and RPPA Parser (RedLang)

This repository provides:

- **FlowNet DSL**: A compact C++ implementation (`engine.hpp` + `engine.cpp`) that compiles a high-level workflow AST into Petri-net models (JSON & PNML).
- **RedLang RPPA Parser**: A DSL parser (`rppa_parser.cpp`) that reads `.rppa` files (using the RPPA syntax) and leverages the FlowNet DSL engine to emit Petri-net modules.

---

## Overview

FlowNet DSL and the RedLang RPPA Parser together allow you to:

Define workflows in a concise text DSL (.rppa).

Parse them into an AST featuring actions, sequences, parallels, choices, priorities, and module calls.

Compile the AST into a Petri-net model (PetriModule) with unique places, transitions, and arcs.

Export JSON and PNML formats for visualization, analysis, or execution.

FlowNet DSL and the RedLang RPPA Parser together allow you to:

- **Define** workflows in a concise text DSL (`.rppa`).
- **Parse** them into an AST featuring actions, sequences, parallels, choices, priorities, and module calls.
- **Compile** the AST into a Petri-net model (`PetriModule`) with unique places, transitions, and arcs.
- **Export** JSON and PNML formats for future visualization, analysis, or execution.


Inspired by classical process calculi like CCS and CSP, RPPA supports:

- **Sequential composition**

- **Parallel execution (||)**

- **Non-deterministic choice (|)**

- **Prioritized execution (^n)**

- **Encapsulated modules (via ModuleName())**

Each RPPA expression derives a compositional structure of transitions and places that can be visualized or executed as a Petri net. This derivation process is formalized through recursive descent parsing and AST-to-Petri-net compilation.




---

## File Structure

```text
/root
├── engine.hpp             # AST node and PetriModule definitions
├── engine.cpp             # CodegenVisitor implementation
├── rppa_parser.cpp        # DSL parser + CLI
├── module.rppa.json       # Optional: manifest listing RPPA files
├── rppa.lock              # Optional: lockfile for module registration
└── json/
    └── include/           # nlohmann/json headers
```

---

## Installation & Build

1. **Install dependencies**:
   - A C++17 compiler
   - [nlohmann/json](https://github.com/nlohmann/json)

2. **Compile**:

   ```bash
   g++ -std=c++17 -O2 -Wall \
       -Ijson/include \
       engine.cpp rppa_parser.cpp -o redlang
   ```

3. **Run CLI**:

   ```bash
   ./redlang
   ```

   ```text
   Usage: parser --compile <file.rppa> <module> \
           | --manifest <json> | --lock <lockfile> | --test
   ```

---

## Testing

Run the built-in parser tests:

```bash
./redlang --test
```

Expected output:

```text
OK: a.b.c
OK: a||b
OK: [a||b]
OK: x^5
OK(invalid): a|b
OK(invalid): .a
OK(invalid): [a.b
OK(invalid): a^^2
```

---

## Compiling a Workflow

**myflow.rppa**
```bash
[ [ x.y ]^1 || z ]^2 
. alpha()^3 
|| [ [ m.n ]^5 || [ o.p.q ]^2 ]^4 
. final_cleanup()^8 
. cleanup
```

To parse and compile an RPPA file:

```bash
./redlang --compile myflow.rppa mymod
```

This emits:

```text
Emitted mymod.json and mymod.pnml
```


The parser reads your .rppa file and converts it into an abstract syntax tree (AST).The AST is passed to the code generation engine (CodegenVisitor) which:

- **Assigns unique place and transition IDs**

- **Builds a Petri-net model by wiring transitions and places**

- **Honors priority (^) and module calls**

The final PetriModule is emitted in:

mymod.json — a structured JSON containing:
- **moduleName : entry and exit**
- **lists of places, transitions (with priority), and arcs (with types)**


mymod.pnml — an XML file conforming to PNML schema, with
- **<place>, <transition>, <arc>**
- **custom toolspecific sections to embed priorities and arc types**



---

## Lockfile Registration

To register modules from a lockfile:

```bash
./redlang --lock rppa.lock
```

Output:

```text
Registered modules from lockfile
```

---

## Manifest

To display a manifest file:

```bash
./redlang --manifest module.rppa.json
```

Example manifest (`module.rppa.json`):

```json
{
  "author": "Kamalesh",
  "description": "Example RPPA manifest",
  "name": "example_module",
  "version": "1.0"
}
```

---

## CLI Summary

| Command                              | Description                                                                      |
|--------------------------------------|----------------------------------------------------------------------------------|
| `--compile <file> <module>`         | Parse and compile `file.rppa` into `<module>.json` and `<module>.pnml`          |
| `--manifest <json>`                 | Load and print an RPPA manifest file                                             |
| `--lock <lockfile>`                 | Register modules listed in a lockfile                                            |
| `--test`                            | Run parser unit tests                                                            |

---

