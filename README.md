FlowNet DSL and RPPA Parser(RedLang)

This repository provides:

FlowNet DSL: A compact C++ implementation (engine.hpp + engine.cpp) that compiles a high-level workflow AST into Petri-net models (JSON & PNML).

RedLang RPPA Parser: A DSL parser (rppa_parser.cpp) that reads .rppa files (using the RPPA syntax) and leverages the FlowNet DSL engine to emit Petri-net modules.

Overview:

FlowNet DSL and the RPPA Parser together allow you to:

Define workflows in a concise text DSL (.rppa).

Parse into an AST with actions, sequences, parallels, choices, priorities, and module calls.

Compile into a Petri-net model (PetriModule) with unique places, transitions, and arcs.

Export JSON and PNML formats for visualization, analysis, or execution.

File Structure:

./root
├── engine.hpp             # AST node and PetriModule definitions
├── engine.cpp             # CodegenVisitor implementation
├── rppa_parser.cpp        # DSL parser + CLI
├── module.rppa.json          # Optional: list of RPPA files
├── rppa.lock           # Optional: lockfile for module registration
└── json/
    └── include/..json.hpp     # Sample RPPA DSL file

Installation & Build

Install dependencies:

A C++17 compiler

nlohmann/json


TO RUN:
>g++ -std=c++17 -O2 -Wall -Ijson/include -o redlang rppa_parser.cpp


>redlang
Usage: parser --compile <file.rppa> <module> | --manifest <json> | --lock <lockfile> | --test

TEST:
>redlang --test
OK: a.b.c
OK: a||b
OK: [a||b]
OK: x^5
OK(invalid): a|b
OK(invalid): .a
OK(invalid): [a.b
OK(invalid): a^^2


COMPILE:
>redlang --compile myflow.rppa mymod
Emitted mymod.json and .pnml


LOCK:
>redlang --lock rppa.lock
Registered modules from lockfile


MANIFEST:
>redlang --manifest module.rppa.json
{
  "author": "Kamalesh",
  "description": "Example RPPA manifest",
  "name": "example_module",
  "version": "1.0"
}
