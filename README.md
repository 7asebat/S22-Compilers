# S22-Compilers
Language compiler in flex+bison

## Get started
```sh
$ make compile
$ make run
```

## Syntax
[Full syntax example](examples/full.program)

## Checklist
### Phase 1
- [x] [Lexer](compiler/Lexer.l)
- [x] [Parser](compiler/Parser.y)
- [x] Simple syntax error handler

### Phase 2
- [x] Design a suitable and extensible format for the ast_symbol table
- [x] Design suitable action rules to produce the output quadruples
- [x] Implement a proper syntax error handler
- [x] Build a simple semantic analyzer
- [x] Implement a simple GUI to insert the input source code and show the output quadruples, ast_symbol table, and syntax and semantic errors (if exist).