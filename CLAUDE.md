# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands
- Compile for Sokol: `make sokol`
- Compile for SDL: `make sdl`
- Compile for WASM: `make wasm`
- Note: high_impact must be compiled together with your game code (not as standalone library)

## Code Style Guidelines
- Use tabs for indentation (not spaces)
- Use snake_case for variables, functions, types
- Types have _t suffix: `vec2_t`, `rgba_t`, `engine_t`
- Functions follow pattern: `module_action()` or `type_action()`
- Opening braces on same line as statement: `if (condition) {`
- Fatal errors handled with `error_if(condition, message, ...)`
- Use `static inline` for small utility functions
- Types defined using typedefs with struct/union
- Comments use `//` style, sparse but targeted for complex code
- Header files include purpose documentation in block comments
- Boolean variables prefixed with `is_`: `is_running`
- Constants/macros use UPPERCASE: `ENGINE_MAX_TICK`

## Memory Management
- Use bump allocator pattern for memory (no malloc/free)
- Check header files for module documentation and API usage