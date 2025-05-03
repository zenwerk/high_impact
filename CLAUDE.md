# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands
- Compile for Sokol: `make sokol`
- Compile for SDL: `make sdl`
- Compile for WASM: `make wasm`
- Note: high_impact must be compiled together with your game code (not as standalone library)
- See examples in [Biolab Disaster](https://github.com/phoboslab/high_biolab) or [Drop](https://github.com/phoboslab/high_drop)

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
- Macros for utility functions use statement expressions with type safety checks

## Memory Management
- Custom memory management system - no malloc/free
- Bump allocator for frame/scene lifetime objects (grows linearly from start of memory block)
- Temp allocator for short-lived objects (grows from end of memory block)
- Memory reset at well-defined points (scene changes, frame end)
- Explicit check for memory leaks at frame end via `temp_alloc_check()`
- Use `alloc_pool()` for scoped memory allocation

## Engine Architecture
- Engine is a framework, not a library - high_impact calls your game code
- Scene-based game organization with init/update/draw/cleanup lifecycle
- Entity-component system with vtable-like design for entity types
- Camera system with deadzone and lookahead features
- Input and rendering systems are abstracted over different backends
- Asset loading limited to QOI for images and QOA for audio
- Uses weltmeister.html for level editing

## Rendering System
- Multiple renderer backends (OpenGL, Metal, Software) for cross-platform support
- Renderer abstraction layer in `render.h` and `render.c`
- Platform-specific implementations in `render_gl.c`, `render_metal.m`, and `render_software.c`
- Logical resolution vs. physical screen resolution with configurable scaling modes
- Transform stack for hierarchical positioning, rotation, and scaling
- Post-processing effects support including CRT simulation
- Texture atlas for efficient batch rendering
- Blend modes for normal and additive transparency

## Debugging
- The engine will terminate with clear error messages on allocation failures
- Error handling follows "fail fast" principle
- No exception handling - invalid conditions terminate program
- Performance statistics tracked in `engine.perf` structure

## Project Structure
- `src/` - Engine core code
- `libs/` - External dependencies (bundled)
- Entry point is through `main_init()` that must be implemented by the game

## Japanese Comments
- Many source files contain Japanese comments for educational purposes
- Comments explain core concepts for readers with no prior graphics or game programming experience
- Comments maintain a consistent style with initial concept overview followed by detailed explanations