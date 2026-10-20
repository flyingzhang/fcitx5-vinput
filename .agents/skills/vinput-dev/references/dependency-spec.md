# fcitx5-vinput 第三方依赖管理与现代化治理规范

**文档状态**：草案 / 架构共识
**关联议题**：[#407 (RFC: 全面基础设施现代化方案)](https://github.com/xifan2333/fcitx5-vinput/issues/407) 及系列子 Issue (#410~#420)
**适用范围**：`fcitx5-vinput` 核心代码库、`flake.nix`、CMake 构建系统与多发行版打包流

---

## 1. 背景与核心痛点 (Context & Problem Statement)

在 `fcitx5-vinput` 推进 C++20 现代化的过程中，手写底层胶水代码与外来依赖管理暴露了三类深层次问题：

1. **手写基础设施的维护黑洞**：早期手写的大量基础设施代码（如 Unicode 东亚字符宽度区间表、Base64 查表、变参 `snprintf` 格式化、Socket/WS 协议等）随着 Unicode 标准扩充和业务复杂度提升，维护成本极高且极易出现字符劈裂、内存违规等失控隐患。
2. **离线沙箱构建被网络下载阻断**：使用 CMake `FetchContent(GIT_REPOSITORY ...)` 默认行为会在编译阶段尝试 `git clone`，而 NixOS 沙箱、Flatpak 离线构建环境、Arch clean chroot 均严格禁止网络访问，直接导致 `could not find git for clone` 构建崩溃。
3. **第三方代码风格污染项目代码体检**：外部引入的 C/C++ 库如果直接作为项目源码编译或普通包含，其内部的类型强转（如 `reinterpret_cast`）或老旧语法会触发项目的严格静态检查（`clang-tidy`），破坏 Hard Constraint 10（零容忍压制告警）。

---

## 2. 核心架构：三层依赖双轨制治理策略 (Three-Tier Strategy)

依据库的成熟度、体量以及主流 Linux 发行版（NixOS、Debian/Ubuntu、Arch）的官方打包支持度，将所有第三方依赖严格划分为三层，采用“双轨制”规范化治理：

```text
                        fcitx5-vinput 依赖治理架构
 ┌───────────────────────────────────────────────────────────────────────┐
 │ Layer 1: 系统核心服务（System-Only）                                    │
 │   - 代表：fcitx5, pipewire, libsystemd, qt6                           │
 │   -> 策略：严格由宿主系统包管理器提供，CMake find_package REQUIRED          │
 ├───────────────────────────────────────────────────────────────────────┤
 │ 轨道 1 (Layer 2): 主流跨发行版通用库 (System-Distributed)               │
 │   - 代表：cpp-httplib, spdlog, CLI11, nlohmann_json, reproc, tl-expected │
 │   -> 策略：各发行版配置直接声明，CMake find_package，离线友好               │
 ├───────────────────────────────────────────────────────────────────────┤
 │ 轨道 2 (Layer 3): 轻量专用 Header-Only 库 (Vendored 3rdparty)          │
 │   - 代表：base64.hpp, unicodelib.h, readerwriterqueue.h, mio.hpp       │
 │   -> 策略：内嵌至 3rdparty/，声明为 SYSTEM INTERFACE，绝对离线与 Linter 免疫 │
 └───────────────────────────────────────────────────────────────────────┘
```

---

## 3. #407 全量现代化候选库分布与归轨清单

| 基础设施模块 | 推荐库 | 主流发行版支持度 (Nixpkgs / Debian / Arch) | 归属轨道 | 接入方式与规范 |
| :--- | :--- | :--- | :--- | :--- |
| **网络通信** (#410) | `cpp-httplib` | ✅ `pkgs.httplib` / `libhttplib-dev` / `cpp-httplib` | **轨道 1** | `flake.nix` 声明 `pkgs.httplib`，CMake `find_package(httplib)` |
| **结构化日志** (#411) | `spdlog` | ✅ `pkgs.spdlog` / `libspdlog-dev` / `spdlog` | **轨道 1** | `flake.nix` 声明 `pkgs.spdlog`，以 `#define SPDLOG_HEADER_ONLY` 消费 |
| **命令行解析** | `CLI11` | ✅ `pkgs.cli11` / `libcli11-dev` / `cli11` | **轨道 1** | `flake.nix` 声明 `pkgs.cli11`，CMake `find_package(CLI11)` |
| **JSON 序列化** | `nlohmann_json` | ✅ `pkgs.nlohmann_json` / `nlohmann-json3-dev` | **轨道 1** | `flake.nix` 声明 `pkgs.nlohmann_json` |
| **子进程管理** (#417) | `reproc` | ✅ `pkgs.reproc` / `libreproc-dev` / `reproc` | **轨道 1** | `flake.nix` 声明 `pkgs.reproc`，CMake `find_package(reproc)` |
| **错误处理单子** (#414)| `tl-expected` | ✅ `pkgs.tl-expected` / `libtl-expected-dev` | **轨道 1** | `flake.nix` 声明 `pkgs.tl-expected` |
| **Base64 编解码** (#416)| `tobiaslocker/base64` | ❌ 各发行版均无单独包 | **轨道 2** | 内嵌为 `3rdparty/base64/base64.hpp` (单头文件, ~300行) |
| **Unicode 字宽/CJK** (#416)| `yhirose/cpp-unicodelib` | ❌ 各发行版无单独包 | **轨道 2** | 内嵌为 `3rdparty/unicodelib/unicodelib.h` (单头文件) |
| **音频无锁队列** (#413)| `readerwriterqueue` | ⚠️ Nix 部分源有，Debian/Ubuntu 缺 | **轨道 2** | 内嵌为 `3rdparty/readerwriterqueue/` (单头文件, ~600行) |
| **零拷贝 mmap** (#412) | `mio` | ⚠️ 覆盖不全，Debian 缺 | **轨道 2** | 内嵌为 `3rdparty/mio/` (单头文件, ~800行) |
| **CLI 终端表格排版** (#420)| `tabulate` / `indicators` | ⚠️ 覆盖率低，Debian 缺 | **轨道 2** | 内嵌为 `3rdparty/tabulate/` (纯展示单头文件) |

---

## 4. 轨道标准接入与构建 SOP

### 4.1 轨道 1（系统级库）配置规范

1. **`flake.nix` 声明**：
   在 `buildInputs` 中追加对应的 nix package，严禁在 derivation 编译时联网：
   ```nix
   buildInputs = with pkgs; [
     fcitx5
     systemdLibs
     pipewire
     httplib
     spdlog
     cli11
     nlohmann_json
     reproc
     tl-expected
   ];
   ```
2. **CMake 构建配置**：
   ```cmake
   find_package(reproc QUIET)
   if(NOT reproc_FOUND)
       message(FATAL_ERROR "reproc not found. Please install libreproc-dev or pkgs.reproc.")
   endif()
   ```

### 4.2 轨道 2（内嵌 3rdparty 库）配置规范

1. **目录结构规范**：
   ```text
   3rdparty/
   ├── <lib_name>/
   │   ├── <lib_name>.hpp (或 .h)
   │   └── LICENSE (保留上游开源协议文本)
   ```
2. **CMake `SYSTEM INTERFACE` 强制隔离**：
   必须使用 `SYSTEM` 关键字，确保编译器将该目录视作系统头文件，从而自动免除 `clang-tidy` 和严格警告扫描：
   ```cmake
   # 以 base64 和 unicodelib 为例
   add_library(3rdparty_base64 INTERFACE)
   target_include_directories(3rdparty_base64 SYSTEM INTERFACE
       ${CMAKE_SOURCE_DIR}/3rdparty/base64
   )
   add_library(base64::base64 ALIAS 3rdparty_base64)

   add_library(3rdparty_unicodelib INTERFACE)
   target_include_directories(3rdparty_unicodelib SYSTEM INTERFACE
       ${CMAKE_SOURCE_DIR}/3rdparty/unicodelib
   )
   add_library(unicodelib::unicodelib ALIAS 3rdparty_unicodelib)
   ```
3. **开源法务与合规登记 (`THIRD_PARTY_NOTICES.md`)**：
   每纳入一个 `3rdparty` 库，必须在 `THIRD_PARTY_NOTICES.md` 登记：
   - 库名称与 Upstream GitHub 地址；
   - 纳入的 Commit/Tag 版本及导入日期；
   - 开源许可证（如 MIT、Apache-2.0 等）。

---

## 5. 工具层重构准则：杜绝手写与过度封装

1. **零手写基础设施**：
   - 不再手写底层状态机、查找表、字符大小写位运算、码点区间表；
   - UTF-8 字宽、字符分类、CJK 判定全部交由成熟 Unicode 库（`unicodelib`）标准查表。
2. **拒绝无意义的多重转发门面（YAGNI 原则）**：
   - 单点业务使用（例如 Base64 仅用于流式 PCM 转 JSON）：直接在业务调用处引入头文件使用，不在公共工具类中维护多重重载；
   - 公共工具类（如 `string_utils.h`）仅保留真正的全项目共享能力，代码量控制在几十行以内；
3. **复合算法才进行封装**：
   - 仅对跨库组装的高阶复合业务算法（如同时依赖“字形簇边界探测 + 界面宽度预算分配 + 居中插入省略号”的 `TruncateMiddleUtf8`）进行精简封装，内部逻辑完全由外部库能力驱动。
