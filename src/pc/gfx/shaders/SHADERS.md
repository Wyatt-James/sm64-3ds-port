# N3DS Shader Compilation Model

Example Shader Program Hierarchy:
```
src/pc/gfx/shaders
  /example
    example.v.pica
    example2.v.pica
    example.g.pica
  /example.shprog
```
example.shprog:
```
example/example.v.pica
example/example2.v.pica
example/example.g.pica
```
- Only the root `shaders` directory is searched for `.shprog` files.
- `.shprog` file paths are relative to the root `shaders` directory.
- `.shprog` file paths cannot contain spaces.
- `.shprog` file paths can be separated either by spaces or by newlines.
- The distinction between vertex and geometry shader file extensions is not enforced.
- Shaders are passed to Picasso in the order that they are listed in the file. This means that their DVLEs also follow this order.
- Despite the fact that all linked shaders share uniform space, since uniform locations are stored per-DVLE, it's a good idea to declare ALL uniforms in at least one file; generally, use the first in the linking order.
