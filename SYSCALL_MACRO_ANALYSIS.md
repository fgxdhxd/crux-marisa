# SYSCALL 宏体系版本冲突分析与解决方案

## 当前问题概述

你的内核树混合了**两个不同时期的 SYSCALL 框架**：

1. **旧框架**（`include/linux/compat.h`）：基于 alias 机制，使用 `compat_sys_xxx` 函数名
2. **新框架**（`arch/arm64/include/asm/syscall_wrapper.h`）：基于 pt_regs 包装，使用 `__arm64_sys_xxx`/`__arm64_compat_sys_xxx` 函数名

---

## 版本差异对比

### 1. 基础宏定义层（✓ 统一，位置：`include/linux/syscalls.h`）

| 宏 | 定义位置 | 作用 | 状态 |
|---|--------|------|------|
| `__MAP0-6` | syscalls.h | 参数映射 | ✓ 统一 |
| `__SC_DECL` | syscalls.h | 声明格式 | ✓ 统一 |
| `__SC_LONG` | syscalls.h | 长整数类型转换 | ✓ 统一 |
| `__SC_CAST` | syscalls.h | 强制类型转换 | ✓ 统一 |
| `__SC_TEST` | syscalls.h | 编译时类型检查 | ✓ 统一 |
| `__SC_ARGS` | syscalls.h | 参数提取 | ✓ 统一 |
| `__SC_DELOUSE` | compat.h | 类型洗白（compat only） | ✓ 统一 |
| `__PROTECT` | syscalls.h | 参数保护 | ✓ 统一 |

### 2. 高层宏定义层（✗ 冲突）

#### SYSCALL_DEFINE 系列

**旧版本**（`syscalls.h` fallback，当 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=n` 时）：
```c
#define SYSCALL_DEFINE0(sname)
	SYSCALL_METADATA(_##sname, 0);
	asmlinkage long sys_##sname(void);
	ALLOW_ERROR_INJECTION(sys_##sname, ERRNO);
	asmlinkage long sys_##sname(void)

#define __SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))
	__attribute__((alias(__stringify(SyS##name))));
	...
```
**函数名**：`sys_foo`, `SyS_foo` 内部实现

**新版本**（`arch/arm64/include/asm/syscall_wrapper.h`，当 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y` 时）：
```c
#define SYSCALL_DEFINE0(sname)
	SYSCALL_METADATA(_##sname, 0);
	asmlinkage long __arm64_sys_##sname(void);
	ALLOW_ERROR_INJECTION(__arm64_sys_##sname, ERRNO);
	asmlinkage long __arm64_sys_##sname(void)

#define __SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long __arm64_sys##name(const struct pt_regs *regs);
	...
```
**函数名**：`__arm64_sys_foo`，接收 `const struct pt_regs *regs`

#### COMPAT_SYSCALL_DEFINE 系列

**旧版本**（`include/linux/compat.h` 总是被使用）：
```c
#define COMPAT_SYSCALL_DEFINE0(name)
	asmlinkage long compat_sys_##name(void);
	ALLOW_ERROR_INJECTION(compat_sys_##name, ERRNO);
	asmlinkage long compat_sys_##name(void)

#define COMPAT_SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));
	asmlinkage long compat_sys##name(...)
	__attribute__((alias(__stringify(compat_SyS##name))));
	...
```
**函数名**：`compat_sys_foo`, `compat_SyS_foo` 内部实现

**新版本**（`arch/arm64/include/asm/syscall_wrapper.h`）：
```c
#define COMPAT_SYSCALL_DEFINE0(sname)
	asmlinkage long __arm64_compat_sys_##sname(void);
	ALLOW_ERROR_INJECTION(__arm64_compat_sys_##sname, ERRNO);
	asmlinkage long __arm64_compat_sys_##sname(void)

#define COMPAT_SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long __arm64_compat_sys##name(const struct pt_regs *regs);
	...
```
**函数名**：`__arm64_compat_sys_foo`，接收 `const struct pt_regs *regs`

---

## 问题根源

### 当前状态
```
include/linux/compat.h          arch/arm64/include/asm/syscall_wrapper.h
      ↓                                      ↓
  旧版本宏定义              新版本宏定义（带#ifdef CONFIG_COMPAT）
  生成 compat_sys_foo      但文件被#include到 syscalls.h 后
                           会覆盖 compat.h 的定义（如果CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y）
```

### 具体表现

1. **宏重定义警告**：
   - `arch/arm64/include/asm/syscall_wrapper.h:18` 的 `COMPAT_SYSCALL_DEFINEx` 
   - 覆盖了 `include/linux/compat.h:52` 的定义

2. **函数未生成**：
   - 旧框架期望生成 `sys_mount`，新框架生成 `__arm64_sys_mount`
   - 编译器找不到 `sys_mount`，提示用 `ksys_mount`

3. **元数据不一致**：
   - `SYSCALL_METADATA` 被重复定义或忽略

---

## 正确的更新策略

### 策略 A：采纳新框架（推荐，面向未来）

#### 前提条件
- `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y`（arm64 默认启用）
- 所有 arch 特定实现都要更新

#### 文件更新顺序

**1. `include/linux/syscalls.h`**
- **操作**：确保基础宏定义完整且不被覆盖
- **检查点**：
  - `__MAP0-6` ✓
  - `__SC_DECL`, `__SC_LONG`, `__SC_CAST`, `__SC_TEST`, `__SC_ARGS` ✓
  - `__PROTECT` ✓
  - `SYSCALL_METADATA` 定义完整 ✓
  - 当 `#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 时，不定义默认的 `SYSCALL_DEFINE0` 和 `__SYSCALL_DEFINEx` ✓

**2. `arch/arm64/include/asm/syscall_wrapper.h`**
- **操作**：确保定义了所有必要的宏
- **检查点**：
  - `__SYSCALL_DEFINEx` 接收 `const struct pt_regs *regs` ✓
  - `COMPAT_SYSCALL_DEFINEx` 同样接收 `const struct pt_regs *regs` ✓
  - `SC_ARM64_REGS_TO_ARGS` 正确提取参数 ✓
  - `COND_SYSCALL`, `COND_SYSCALL_COMPAT` 宏定义 ✓
  - `SYS_NI`, `COMPAT_SYS_NI` 宏定义 ✓

**3. `include/linux/compat.h`**
- **操作**：删除或条件编译 COMPAT_SYSCALL_DEFINE* 宏定义
- **模式**：
  ```c
  #ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
  /* 旧版本的 COMPAT_SYSCALL_DEFINE* 定义 */
  #define COMPAT_SYSCALL_DEFINE0(name) ...
  #define COMPAT_SYSCALL_DEFINEx(x, name, ...) ...
  #endif
  ```
- 这样当 arm64 包含 `syscall_wrapper.h` 时，`compat.h` 的定义会被跳过

**4. `include/linux/unistd.h` 或 `arch/arm64/include/uapi/asm/unistd.h`**
- **操作**：确保 `__NR_*` 号与系统调用表一致
- **检查点**：所有 syscall 的编号未更改

**5. `arch/arm64/kernel/sys_ni.c`（如果存在）**
- **操作**：使用新宏 `COND_SYSCALL`, `COND_SYSCALL_COMPAT`
- **模式**：
  ```c
  #ifdef CONFIG_COMPAT
  COND_SYSCALL_COMPAT(foo);
  #endif
  COND_SYSCALL(foo);
  ```

**6. 系统调用表（`arch/arm64/kernel/syscall_64.S` 或 `.tbl` 文件）**
- **操作**：无需更改，表中仍引用 `sys_foo` 的符号名
- **链接器** 会自动找到 `__arm64_sys_foo`（通过 SYSCALL_ALIAS 或符号重映射）

### 策略 B：保持旧框架（不推荐，过时）

#### 前提条件
- `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=n`

#### 文件更新顺序

**1. `include/linux/syscalls.h`**
- **操作**：删除或禁用 `#include <asm/syscall_wrapper.h>`

**2. `arch/arm64/include/asm/syscall_wrapper.h`**
- **操作**：删除或留空（不被包含）

**3. `include/linux/compat.h`**
- **操作**：保持旧版本的 COMPAT_SYSCALL_DEFINE* 宏

---

## 最小改动方案（推荐操作）

假设你要**完全采纳新框架**，以下是最小改动步骤：

### Step 1：确保 include/linux/syscalls.h 正确

**检查**：`CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y` 时，确保 fallback 宏被跳过

```c
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
#include <asm/syscall_wrapper.h>
#endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */

/* __MAP 和基础宏始终定义 */
#define __MAP0(m,...)
#define __MAP1(m,t,a,...) m(t,a)
... （总是定义）

/* 这些只在 !CONFIG_ARCH_HAS_SYSCALL_WRAPPER 时定义 */
#ifndef __SYSCALL_DEFINEx
#define __SYSCALL_DEFINEx(...) ...  /* 旧版本 */
#endif
```

**验证**：编译时检查 `__SYSCALL_DEFINEx` 是否来自 `syscall_wrapper.h`（新版本）

### Step 2：修改 include/linux/compat.h

**替换**：
```c
/* 旧版本的 COMPAT_SYSCALL_DEFINE* 定义之前添加 */
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER

#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	ALLOW_ERROR_INJECTION(compat_sys_##name, ERRNO); \
	asmlinkage long compat_sys_##name(void)

#define COMPAT_SYSCALL_DEFINEx(x, name, ...) \
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));...

#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
```

**效果**：当 arm64 打开 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 时，compat.h 的宏会被跳过，由 `syscall_wrapper.h` 提供

### Step 3：确保 arch/arm64/include/asm/syscall_wrapper.h 完整

**检查**：所有必要的宏都已定义（通常已有）
```c
- SYSCALL_DEFINE0
- __SYSCALL_DEFINEx
- COMPAT_SYSCALL_DEFINE0
- COMPAT_SYSCALL_DEFINEx
- COND_SYSCALL
- COND_SYSCALL_COMPAT
- SYS_NI
- COMPAT_SYS_NI
```

### Step 4：检查 arch/arm64/kernel/syscall_64.S 和 sys_ni.c

**目标**：确保没有仍在使用旧的 `sys_*` 函数名调用

---

## 验证清单

### 编译验证
```bash
# 1. 检查宏定义来源
gcc -E -dM kernel/sys_ni.c 2>&1 | grep -A2 "SYSCALL_DEFINE"
gcc -E -dM fs/mount.c 2>&1 | grep -A2 "__SYSCALL_DEFINEx"

# 2. 检查函数符号生成
nm vmlinux | grep sys_mount     # 应该找到 __arm64_sys_mount 或 sys_mount
nm vmlinux | grep compat_sys    # 应该找到 __arm64_compat_sys_* 或 compat_sys_*

# 3. 完整编译
make -j$(nproc) 2>&1 | grep -i "macro redefined\|implicit declaration"
```

### 运行时验证
```bash
# 查看系统调用表是否正确
cat /proc/sys/kernel/syscall_* 2>/dev/null

# strace 验证系统调用工作
strace -e mount ls /
```

---

## 推荐决策

**你应该采纳方案 A（新框架）**，原因：

1. ✓ arm64 内核已原生支持 pt_regs 包装机制
2. ✓ 新框架效率更高（无 alias 开销）
3. ✓ 符合现代内核设计（>=4.17）
4. ✓ 你的编译选项已启用 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER`

**最小改动**：修改 `include/linux/compat.h`，条件编译旧宏定义（Step 2）

---

## 常见问题

**Q1: 为什么还有旧框架？**
A: 兼容不支持 pt_regs 包装的 arch（x86 旧版本等）

**Q2: `ALLOW_ERROR_INJECTION` 可以为空吗？**
A: 可以（已有 fallback 定义在 syscalls.h），但建议保留以支持错误注入调试

**Q3: `compat_sys_*` vs `__arm64_compat_sys_*` 如何共存？**
A: 通过 `SYSCALL_ALIAS` 宏创建符号别名，编译器链接时选择正确的符号

**Q4: 能混用两个框架吗？**
A: 不能。会导致相同符号多重定义，编译失败。

