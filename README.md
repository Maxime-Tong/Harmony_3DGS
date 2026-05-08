# HarmonyOS 3D Gaussian Splatting (3DGS) with Vulkan

[![HarmonyOS](https://img.shields.io/badge/HarmonyOS-Next-blue)](https://developer.harmonyos.com/)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-purple)](https://www.vulkan.org/)
[![C++](https://img.shields.io/badge/C++-17-blue)](https://isocpp.org/)
[![ArkTS](https://img.shields.io/badge/ArkTS-TypeScript-green)](https://developer.harmonyos.com/en/docs/documentation/doc-guides/arkts-get-started-0000001724245218)

A high-performance implementation of 3D Gaussian Splatting (3DGS) rendering algorithm on HarmonyOS using Vulkan graphics API. This project demonstrates real-time neural rendering capabilities on mobile devices with HarmonyOS.

## 🎯 Features

- **Real-time 3D Gaussian Splatting Rendering**: Implements the state-of-the-art 3DGS algorithm for photorealistic novel view synthesis
- **Vulkan Graphics API**: Leverages Vulkan for cross-platform, high-performance graphics rendering on HarmonyOS
- **HarmonyOS Native Integration**: Native C++ implementation with ArkTS/JS bindings through NAPI
- **Efficient GPU Computing**: Utilizes compute shaders for parallel processing of Gaussian primitives
- **Memory Optimized**: Implements tile-based rendering with efficient sorting and culling
- **Cross-platform Scene Loading**: Supports PLY format point cloud loading with JSON camera configurations

## 📁 Project Structure

```
HarmonyOS_3DGS/
├── entry/                          # Main application module
│   ├── src/main/
│   │   ├── cpp/                    # Native C++ implementation
│   │   │   ├── napi_init.cpp       # NAPI bindings for ArkTS/JS
│   │   │   ├── render/             # Vulkan rendering engine
│   │   │   │   ├── include/        # Header files
│   │   │   │   │   ├── Buffer.h             # Vulkan buffer management
│   │   │   │   │   ├── Context.h            # Vulkan context and device management
│   │   │   │   │   ├── DescriptorSet.h      # Vulkan descriptor sets
│   │   │   │   │   ├── GSScene.h            # 3D scene and PLY loader
│   │   │   │   │   ├── PluginRender.h       # HarmonyOS XComponent integration
│   │   │   │   │   ├── Shader.h             # Shader compilation and management
│   │   │   │   │   ├── Swapchain.h          # Swapchain and surface management
│   │   │   │   │   ├── Vulkan3DGS.h         # Main 3DGS rendering class
│   │   │   │   │   ├── XEngineSorter.h      # Radix sort implementation
│   │   │   │   │   └── pipelines/           # Compute pipeline implementations
│   │   │   │   └── src/             # Implementation files
│   │   │   ├── shaders/             # SPIR-V shader binaries
│   │   │   │   ├── shaders.h        # Main rendering shaders
│   │   │   │   └── shaders_half.h   # Half-precision optimized shaders
│   │   │   └── third_party/         # Third-party libraries
│   │   │       └── glm/             # OpenGL Mathematics library
│   │   ├── ets/                     # ArkTS/TypeScript frontend
│   │   │   ├── entryability/        # Application entry ability
│   │   │   ├── entrybackupability/  # Backup ability
│   │   │   └── pages/               # UI pages
│   │   │       └── Index.ets        # Main rendering page with XComponent
│   │   └── resources/               # Application resources
│   │       ├── base/                # Base resources
│   │       ├── media/               # Media assets
│   │       └── rawfile/             # Raw data files
│   │           ├── cameras.json     # Camera trajectory data
│   │           └── point_cloud.ply  # 3D Gaussian point cloud data
│   └── src/ohosTest/               # Test module
├── AppScope/                       # Application scope resources
│   └── app.json5                   # Application configuration
└── build-profile.json5             # Build configuration
```

## 🏗️ Core Implementation

### 1. **Vulkan Rendering Engine**
- **Context Management**: Handles Vulkan instance, device, queues, and memory allocation using VMA (Vulkan Memory Allocator)
- **Swapchain Management**: Manages surface presentation and frame synchronization
- **Compute Pipelines**: Implements multiple compute shader pipelines for different stages of 3DGS rendering

### 2. **3D Gaussian Splatting Algorithm**
- **Gaussian Primitive Processing**: Transforms 3D Gaussians to 2D screen space with covariance computation
- **Tile-based Rendering**: Divides screen into tiles for efficient parallel processing
- **Depth Sorting**: Implements radix sort for correct depth ordering of Gaussian primitives
- **Attribute Computation**: Calculates conic opacity, color, and bounding boxes for each Gaussian

### 3. **Scene Management**
- **PLY File Loading**: Parses Stanford PLY format for 3D Gaussian point clouds
- **Camera System**: Supports perspective cameras with configurable FOV, near/far planes
- **Uniform Buffer Management**: Handles camera matrices and rendering parameters

### 4. **HarmonyOS Integration**
- **NAPI Bindings**: Exposes C++ functionality to ArkTS/JS through Node-API
- **XComponent Integration**: Uses OH_NativeXComponent for native rendering surface
- **Resource Management**: Accesses HarmonyOS rawfile resources for scene data

### 5. **Performance Optimizations**
- **Compute Shader Pipelines**: Multiple specialized compute shaders for different rendering stages
- **Memory Efficient Buffers**: Uses Vulkan buffers for GPU data storage
- **Parallel Processing**: Leverages GPU compute capabilities for massive parallelism
- **Tile Culling**: Reduces overdraw by culling Gaussians per tile

## 🚀 Building and Running

### Prerequisites
- HarmonyOS SDK (API 9+)
- DevEco Studio 4.0+
- HarmonyOS device or simulator with Vulkan support

### Build Instructions
1. Open the project in DevEco Studio
2. Configure signing certificates in `build-profile.json5`
3. Build the project using the HarmonyOS build system
4. Deploy to target device or simulator

### Running the Application
1. Launch the application on a HarmonyOS device
2. The main interface shows a black canvas with "Start" and "Stop" buttons
3. Click "Start" to begin 3D Gaussian Splatting rendering
4. The application loads the point cloud from `rawfile/point_cloud.ply`
5. Camera trajectory can be loaded from `rawfile/cameras.json`

## 📊 Technical Details

### Rendering Pipeline
1. **Preprocessing**: Transform 3D Gaussians to screen space, compute 2D covariance
2. **Tile Assignment**: Assign Gaussians to screen tiles based on bounding boxes
3. **Depth Sorting**: Sort Gaussians per tile using radix sort
4. **Rendering**: Alpha blending of sorted Gaussians in front-to-back order
5. **Post-processing**: Final composition and presentation

### Data Structures
- **Uniform Buffer**: Camera matrices, screen dimensions, FOV parameters
- **Vertex Buffer**: Gaussian positions, scales, rotations, spherical harmonics coefficients
- **Covariance Buffer**: Precomputed 3D covariance matrices
- **Attribute Buffer**: Conic opacity, color, radii, AABB, UV coordinates, depth

### Shader Stages
1. **Calibration Shader**: Preprocesses Gaussian primitives
2. **Prefix Sum Shader**: Parallel prefix sum for sorting
3. **Radix Sort Shader**: GPU-based radix sort implementation
4. **Tile Boundary Shader**: Computes tile assignments
5. **Render Shader**: Final rendering and blending

## 📦 Dependencies

### Native Dependencies
- **Vulkan SDK**: Graphics API for HarmonyOS
- **GLM**: OpenGL Mathematics library for vector/matrix operations
- **JSONCPP**: JSON parsing for camera configuration
- **HarmonyOS NDK**: Native development kit for HarmonyOS

### HarmonyOS Frameworks
- **@kit.AbilityKit**: UIAbility and lifecycle management
- **@kit.ArkUI**: XComponent for native rendering
- **@kit.LocalizationKit**: Resource management
- **@kit.PerformanceAnalysisKit**: Logging and performance monitoring

## 📄 License

This project is built upon:
- `video-render`: https://gitcode.com/HarmonyOS_Samples/video-render
- `3DGS.cpp`: https://github.com/shg8/3DGS.cpp

Please refer to the original repositories for their respective licenses.

## 🙏 Acknowledgements

- **HarmonyOS Team** for the video-render sample code
- **shg8** for the 3DGS.cpp reference implementation
- **Bernhard Kerbl, Georgios Kopanas, Thomas Leimkühler, George Drettakis** for the original 3D Gaussian Splatting paper
- **Vulkan Working Group** for the Vulkan graphics API

## 🔮 Future Work

- [ ] Support for real-time camera interaction
- [ ] Multi-resolution Gaussian representation
- [ ] Integration with neural network training pipeline
- [ ] Support for dynamic scene updates
- [ ] AR/VR integration with HarmonyOS AR Engine

## 📞 Contact

For questions or contributions, please open an issue in the repository.