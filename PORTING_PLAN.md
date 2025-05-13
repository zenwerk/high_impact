# High Impact Porting Plan: C to Odin

This document outlines the plan for porting the High Impact game framework from C to Odin-lang. The porting process will follow a logical sequence, focusing on learning game development concepts through the implementation process.

## 1. Project Overview

High Impact is a small but complete 2D game engine with:
- A scene-based architecture
- Entity-component system
- Custom memory management
- Multiple rendering backends
- Platform abstraction
- Input, sound, and map systems

## 2. Porting Strategy

### 2.1 Core Principles

1. **Incremental Approach**: Port the framework component by component, ensuring each part works before moving to the next
2. **Idiomatic Odin**: Use Odin-specific features and patterns rather than direct C translations
3. **Learning Focus**: Understand each component thoroughly before porting
4. **Testing**: Create small test cases for each component to verify functionality

### 2.2 Component Sequence

Port components in this order:

1. **Memory Management System** (`alloc.c/h` ’ `memory.odin`)
   - Foundation for all other components
   - Understand bump allocator and temporary allocator concepts
   - Implement Odin-specific memory management patterns

2. **Core Types** (`types.h` ’ `types.odin`)
   - Define base types (vectors, colors, etc.)
   - Implement utility functions

3. **Platform Abstraction** (`platform.c/h` ’ `platform.odin`)
   - Abstract window, input, and timing functions
   - Target one platform first (likely Sokol)

4. **Rendering System** (`render.c/h` ’ `render.odin`)
   - Port the renderer abstraction
   - Implement one backend first (OpenGL)

5. **Entity System** (`entity.c/h`, `entity_def.h` ’ `entity.odin`)
   - Translate vtable-like design to Odin's interfaces or equivalent
   - Implement entity lifecycle management

6. **Engine Core** (`engine.c/h` ’ `engine.odin`)
   - Port main loop and coordination code
   - Implement scene management

7. **Input System** (`input.c/h` ’ `input.odin`)
   - Handle keyboard, mouse, and gamepad input
   - Map input events to actions

8. **Map System** (`map.c/h` ’ `map.odin`)
   - Level and collision maps
   - JSON loading functionality

9. **Sound System** (`sound.c/h` ’ `sound.odin`)
   - Audio playback and management

10. **Animation & Effects** (`animation.c/h` ’ `animation.odin`)
    - Animation system and visual effects

## 3. Technical Challenges and Solutions

### 3.1 Memory Management

**Challenge**: The C code uses a custom bump allocator and temporary allocator system.

**Approach**:
- Understand how Odin's memory management works
- Implement similar bump and temporary allocators
- Potentially leverage Odin's context system for region-based memory

### 3.2 Function Pointers and VTables

**Challenge**: The entity system uses function pointers for polymorphic behavior.

**Approach**:
- Explore Odin's interfaces or procedure variables
- Consider using Odin's union types and polymorphism
- Maintain the same architecture while using more idiomatic Odin code

### 3.3 Platform Abstraction

**Challenge**: Multiple platform backends (SDL/Sokol) need to be supported.

**Approach**:
- Start with a single backend (Sokol)
- Use Odin's foreign system for C library bindings
- Create clean interfaces that can be implemented for different backends

### 3.4 Renderer Backends

**Challenge**: Multiple rendering backends (OpenGL/Metal/Software).

**Approach**:
- Port one backend first (likely OpenGL)
- Use Odin's abstractions to create clean renderer interfaces
- Implement other backends incrementally

## 4. Learning Path

### 4.1 Component Understanding Phase

For each component:
1. Read and analyze original C code thoroughly
2. Document key concepts and architectural decisions
3. Research Odin-specific approaches
4. Create design before implementation

### 4.2 Implementation Phase

For each component:
1. Create minimal implementation focusing on core functionality
2. Test with simple examples
3. Expand to full feature set
4. Document Odin-specific design decisions

### 4.3 Integration Phase

1. Connect components incrementally
2. Create simple test scenes
3. Verify behavior against original C implementation

## 5. Detailed Component Plans

### 5.1 Memory Management System

**Steps:**
1. Understand C bump allocator and temp allocator
2. Research Odin memory management and allocation strategies
3. Implement `bump_alloc`, `bump_reset`, `temp_alloc`, and `temp_free`
4. Create pool allocation pattern similar to `alloc_pool()`
5. Implement memory leak checking for temp allocations

### 5.2 Core Types

**Steps:**
1. Port vector types (`vec2_t`, etc.)
2. Port color types (`rgba_t`)
3. Implement utility functions
4. Ensure proper operator overloading in Odin

### 5.3 Platform Abstraction

**Steps:**
1. Define platform interface
2. Implement Sokol backend first
3. Set up window creation, timing, and input events
4. Implement file I/O functions

### 5.4 Rendering System

**Steps:**
1. Define renderer interface
2. Implement OpenGL backend
3. Port transformation system
4. Implement sprite and texture handling
5. Add post-processing effects

### 5.5 Entity System

**Steps:**
1. Port entity data structures
2. Implement entity creation and lifecycle
3. Create message handling system
4. Set up collision detection and resolution

## 6. Timeline and Milestones

### Phase 1: Foundation (Memory, Types, Platform)
- Memory management system
- Core types
- Platform abstraction (one backend)

### Phase 2: Rendering
- Renderer interface
- OpenGL backend
- Basic sprite rendering

### Phase 3: Engine Core
- Entity system
- Engine main loop
- Scene management

### Phase 4: Gameplay Systems
- Input handling
- Map loading
- Collision detection

### Phase 5: Audio and Polish
- Sound system
- Animation system
- Additional backends

## 7. Resources

- Odin Documentation: https://odin-lang.org/docs/
- High Impact Source: Read all headers and implementation files
- High Impact Examples: [Biolab Disaster](https://github.com/phoboslab/high_biolab) and [Drop](https://github.com/phoboslab/high_drop)
- Game Engine Architecture references
- Japanese comments in source for learning concepts