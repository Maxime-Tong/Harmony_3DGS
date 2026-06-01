# Agent Instructions for HarmonyOS_3DGS

## Project Summary
This repository implements HarmonyOS 3D Gaussian Splatting (3DGS) rendering with Vulkan. The native rendering core lives under [entry/src/main/cpp/render](entry/src/main/cpp/render), the compute shader sources live under [shaders_comp](shaders_comp), and the shader include entrypoint used by native code is [entry/src/main/cpp/shaders/shaders_half.h](entry/src/main/cpp/shaders/shaders_half.h).

For the broader project structure, build notes, and feature overview, prefer the repository README: [README.md](README.md).

## Working Rules
- Keep changes focused on the native render pipeline unless the task explicitly targets ArkTS or app wiring.
- Treat [entry/src/main/cpp/render](entry/src/main/cpp/render) as the source of truth for 3DGS behavior.
- Treat [shaders_comp](shaders_comp) as the source of truth for compute-stage performance work.
- When changing shader behavior, update the C++ side and the shader include path together so the pipeline stays consistent.
- Prefer linking to existing docs instead of repeating them here.

## Performance and Power Optimization Focus
If a task is about reducing power consumption, start by profiling the hottest path rather than broad refactors.

Primary areas to inspect first:
- [entry/src/main/cpp/render/src/Vulkan3DGS.cpp](entry/src/main/cpp/render/src/Vulkan3DGS.cpp), especially `draw()` and command-buffer recording.
- [shaders_comp/render.comp](shaders_comp/render.comp), which is the main rendering bottleneck.
- [shaders_comp/sort.comp](shaders_comp/sort.comp) and [shaders_comp/prefix_sum.comp](shaders_comp/prefix_sum.comp), which are the next most expensive stages.
- Any buffer uploads, copies, reallocation paths, or per-frame setup in the render loop.

Optimization goals to keep in mind:
- Reduce redundant work inside `draw()` and the render loop.
- Reuse buffers and cached GPU resources where possible.
- Remove unnecessary buffer copies, transitions, and recomputation.
- Avoid per-frame work when scene state or camera state has not changed.
- Prefer shader-side early exits, simpler math, and smaller memory traffic before adding new CPU-side orchestration.
- Preserve visual correctness while reducing bandwidth and dispatch count.

Known effective levers already identified by the user:
- Lower rendering resolution.
- Reduce model size.
- Focus on render shader cost first, then sort/prefix_sum.
- Look for chances to drop buffers or redundant operations in the 3DGS pipeline.

## Suggested Workflow for Future Agents
1. Read the README for project context.
2. Inspect `Vulkan3DGS::draw()` and the command-buffer path.
3. Inspect the compute shaders in `shaders_comp` from render backward.
4. Validate any optimization against power, frame time, and correctness together.
5. Keep changes small and measurable.

## Useful References
- [README.md](README.md) for project overview and build/run notes.
- [entry/src/main/cpp/render/include/Vulkan3DGS.h](entry/src/main/cpp/render/include/Vulkan3DGS.h) for core pipeline state.
- [entry/src/main/cpp/render/include/PluginRender.h](entry/src/main/cpp/render/include/PluginRender.h) for HarmonyOS integration entry points.
