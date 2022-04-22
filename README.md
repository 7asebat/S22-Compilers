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
- [x] [Lexer](src/Lexer.l)
- [x] [Parser](src/Parser.y)
- [x] Simple syntax error handler

### Phase 2
- [ ] [AST](src/AST/AST.h)
- [ ] Design a suitable and extensible format for the symbol table
- [ ] Design suitable action rules to produce the output quadruples
- [ ] Implement a proper syntax error handler
- [ ] Build a simple semantic analyzer
- [ ] Implement a simple GUI to insert the input source code and show the output quadruples, symbol table, and syntax and semantic errors (if exist).