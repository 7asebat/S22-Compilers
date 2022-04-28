# Quadruples

> ### _Notes_
> - `V` is either a GP register (`Rx`) or an immediate value (`#Imm`)
> - `L` is a label
> - `x` is a variable label

| op      | dst  | arg1   | arg2   | Description                                       |
| ------- |------|--------|--------|---------------------------------------------------|
| `LD`    | `Rx` | `x`    |        | Load contents of memory address `x` into `Rx`     |
| `LD`    | `Rx` | `V`    | `(Ry)` | Load contents of memory address `Ry[V]` into `Rx` |
| `ST`    | `x`  | `V`    |        | Stores `V` into memory address `x`                |
| `ST`    | `V1` | `(Rx)` | `V2`   | Stores `V2` into memory address `Rx[V1]`          |
| `PUSH`  |      | `V`    |        | Push `V` onto the stack                           |
| `POP`   | `Rx` |        |        | Pop from the stack into `Rx`                      |
| `OP`    | `Rx` | `V`    |        | `Rx = op V`                                       |
| `OP`    | `Rx` | `V1`   | `V2`   | `Rx = V1 op V2`                                   |
| `BR`    | `L`  |        |        | Branch to `L`                                     |
| `Bcond` | `L`  | `V1`   |        | Branch to `L` on `comp V1`                        |
| `Bcond` | `L`  | `V1`   | `V2`   | Branch to `L` on `V1 comp V2`                     |
| `CALL`  | `F`  |        |        | Push return address onto stack, branch to `F`     |
| `RET`   |      |        |        | Pop return address and branch to it               |