# SYSCALL 宏体系统一 - 完整总结

## 问题陈述

你正在移植一个新的 SYSCALL_DEFINE 宏体系，但遇到了**宏版本冲突**导致的编译和链接问题：

1. **编译警告**：`'COMPAT_SYSCALL_DEFINEx' macro redefined` 
   - 位置：`arch/arm64/include/asm/syscall_wrapper.h:18` vs `include/linux/compat.h:52`

2. **编译错误**：`implicit declaration of function 'sys_mount'`
   - 原因：编译器找不到 `sys_mount` 符号，只有 `__arm64_sys_mount`

3. **根本原因**：混用了**旧（alias-based）和新（pt_regs-based）两套 SYSCALL 框架**

---

## 架构版本对比

### 旧框架（已过时，但仍在 generic syscalls.h 中）

```c
// 位置：include/linux/syscalls.h (fallback) 和 include/linux/compat.h
// 适用场景：CONFIG_ARCH_HAS_SYSCALL_WRAPPER = n

#define SYSCALL_DEFINE2(name, t1, a1, t2, a2)
	SYSCALL_DEFINEx(2, _##name, t1, a1, t2, a2)

#define __SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));
	asmlinkage long sys##name(...) __attribute__((alias(__stringify(SyS##name))));
	asmlinkage long SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))
	{
		long ret = SYSC##name(__MAP(x,__SC_CAST,__VA_ARGS__));
		__MAP(x,__SC_TEST,__VA_ARGS__);
		__PROTECT(x, ret, __MAP(x,__SC_ARGS,__VA_ARGS__));
		return ret;
	}
	static inline long SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))

// 函数签名：sys_mount(const char *dev_name, const char *dir_name, ...)
// 实现位置：SyS_mount (大写)
// 别名关系：sys_mount -> SyS_mount
```

#### 生成的符号
- `sys_mount` (别名)
- `SyS_mount` (实现)

### 新框架（现代，arm64 使用）

```c
// 位置：arch/arm64/include/asm/syscall_wrapper.h
// 适用场景：CONFIG_ARCH_HAS_SYSCALL_WRAPPER = y

#define SYSCALL_DEFINE2(name, t1, a1, t2, a2)
	SYSCALL_DEFINEx(2, _##name, t1, a1, t2, a2)

#define __SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long __arm64_sys##name(const struct pt_regs *regs);
	ALLOW_ERROR_INJECTION(__arm64_sys##name, ERRNO);
	static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__));
	static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));
	asmlinkage long __arm64_sys##name(const struct pt_regs *regs)
	{
		return __se_sys##name(SC_ARM64_REGS_TO_ARGS(x, __VA_ARGS__));
	}
	static long __se_sys##name(__MAP(x,__SC_LONG,__VA_ARGS__))
	{
		long ret = __do_sys##name(__MAP(x,__SC_CAST,__VA_ARGS__));
		__MAP(x,__SC_TEST,__VA_ARGS__);
		__PROTECT(x, ret, __MAP(x,__SC_ARGS,__VA_ARGS__));
		return ret;
	}
	static inline long __do_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))

// 函数签名（pt_regs 包装）：__arm64_sys_mount(const struct pt_regs *regs)
// 实现位置：__do_sys_mount（从 pt_regs 参数提取）
// 别名关系：无（直接调用）
```

#### 生成的符号
- `__arm64_sys_mount` (主要)
- `__se_sys_mount` (长整数参数中间层)
- `__do_sys_mount` (实际实现)

### 关键区别

| 特性 | 旧框架 | 新框架 |
|------|--------|--------|
| 函数名 | `sys_mount` | `__arm64_sys_mount` |
| 参数接收 | 直接参数列表 | `const struct pt_regs *regs` |
| 参数转换 | 在 alias 中 | 在 `SC_ARM64_REGS_TO_ARGS` 宏中 |
| 性能 | 一级间接调用 (alias) | 直接调用 + 内联优化 |
| 用途 | 通用架构 | 性能优化架构（arm64） |

---

## 当前配置状态

### arm64 内核的配置

```
CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y    ✓ 已启用新框架
CONFIG_COMPAT=y                      ✓ compat 系统调用已启用
```

### 包含关系

```
fs/mount.c 中的 SYSCALL_DEFINE2(mount, ...)
    ↓
#include <linux/syscalls.h>
    ↓
/* 预处理器检查 */
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    #include <asm/syscall_wrapper.h>  ← arm64，新宏
#else
    /* fallback __SYSCALL_DEFINEx 定义 */
#endif

/* 同时，compat.h 被某处包含 */
#include <linux/compat.h>
    ↓
COMPAT_SYSCALL_DEFINEx 被定义（旧版本）← ⚠️ 冲突！
```

### 冲突点

```
第一次定义：arch/arm64/include/asm/syscall_wrapper.h:18
	#define COMPAT_SYSCALL_DEFINEx(x, name, ...) \
		asmlinkage long __arm64_compat_sys##name(const struct pt_regs *regs); ...

第二次定义：include/linux/compat.h:52
	#define COMPAT_SYSCALL_DEFINEx(x, name, ...) \
		asmlinkage long compat_sys##name(...); \
		asmlinkage long compat_sys##name(...) __attribute__((alias(...)));  ...

编译器发出警告 → 使用最后一个定义（通常是旧的）
```

---

## 文件依赖清单

### 必须成套更新的文件

#### 1. **核心基础宏定义**（✓ 已完整）
   - 文件：`include/linux/syscalls.h`
   - 宏：`__MAP0-6`, `__SC_DECL`, `__SC_LONG`, `__SC_CAST`, `__SC_TEST`, `__SC_ARGS`, `__PROTECT`
   - 状态：✓ 正确，无需改动

#### 2. **旧框架系统调用宏**（❌ 需要条件编译）
   - 文件：`include/linux/compat.h`
   - 宏：`COMPAT_SYSCALL_DEFINE0`, `COMPAT_SYSCALL_DEFINE1-6`, `COMPAT_SYSCALL_DEFINEx`
   - 问题：无条件定义，与新框架冲突
   - 解决：用 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 包围

#### 3. **新框架系统调用宏**（✓ 已完整）
   - 文件：`arch/arm64/include/asm/syscall_wrapper.h`
   - 宏：`SYSCALL_DEFINE0`, `__SYSCALL_DEFINEx`, `COMPAT_SYSCALL_DEFINE0`, `COMPAT_SYSCALL_DEFINEx`, `COND_SYSCALL`, `COND_SYSCALL_COMPAT`
   - 状态：✓ 正确，无需改动

#### 4. **系统调用条件编译**（✓ 已完整）
   - 文件：`include/linux/syscalls.h` 行 87-91
   - 宏：`#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` ... `#include <asm/syscall_wrapper.h>`
   - 状态：✓ 正确

#### 5. **fallback 宏定义**（✓ 已完整）
   - 文件：`include/linux/syscalls.h` 行 227-242
   - 宏：`#ifndef __SYSCALL_DEFINEx` ... 旧定义 ... `#endif`
   - 状态：✓ 正确，当 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=n` 时使用

### 可选改进但非必需的文件

#### 6. **系统调用 ni 桩**（🟢 建议更新）
   - 文件：`arch/arm64/kernel/sys_ni.c`
   - 用途：为未实现的系统调用生成 -ENOSYS 桩
   - 当前：可能仍使用 `cond_syscall(sys_foo)` 旧宏
   - 改进：改用 `COND_SYSCALL(foo)` 新宏（由 syscall_wrapper.h 提供）
   - 优先级：中（不改也能编译，但新宏更清晰）

#### 7. **系统调用表**（🟢 无需改）
   - 文件：`arch/arm64/kernel/syscall_64.S` 或 `.tbl`
   - 用途：系统调用分发表
   - 状态：符号名仍为 `sys_*`，链接器会找到 `__arm64_sys_*` 的别名或直接映射
   - 改动：不需要改（链接器负责符号重映射）

#### 8. **系统调用号定义**（🟢 无需改）
   - 文件：`arch/arm64/include/uapi/asm/unistd.h`
   - 用途：定义 `__NR_mount` 等常数
   - 状态：无需改动

---

## 解决方案：一文件改动

### 唯一需要改动的文件

**文件**：`include/linux/compat.h`

**位置**：第 33 行（COMPAT_SYSCALL_DEFINE0 定义之前）到第 60 行（COMPAT_SYSCALL_DEFINEx 定义之后）

**操作**：

**Before（第 33-60 行）**：
```c
#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	ALLOW_ERROR_INJECTION(compat_sys_##name, ERRNO); \
	asmlinkage long compat_sys_##name(void)

#define COMPAT_SYSCALL_DEFINE1(name, ...) \
        COMPAT_SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE2(name, ...) \
	COMPAT_SYSCALL_DEFINEx(2, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE3(name, ...) \
	COMPAT_SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE4(name, ...) \
	COMPAT_SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE5(name, ...) \
	COMPAT_SYSCALL_DEFINEx(5, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE6(name, ...) \
	COMPAT_SYSCALL_DEFINEx(6, _##name, __VA_ARGS__)

#define COMPAT_SYSCALL_DEFINEx(x, name, ...)				\
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))\
		__attribute__((alias(__stringify(compat_SyS##name))));  \
	ALLOW_ERROR_INJECTION(compat_sys##name, ERRNO);	\
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))\
	{								\
		return C_SYSC##name(__MAP(x,__SC_DELOUSE,__VA_ARGS__));	\
	}								\
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

**After（第 33-61 行）**：
```c
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER

#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	ALLOW_ERROR_INJECTION(compat_sys_##name, ERRNO); \
	asmlinkage long compat_sys_##name(void)

#define COMPAT_SYSCALL_DEFINE1(name, ...) \
        COMPAT_SYSCALL_DEFINEx(1, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE2(name, ...) \
	COMPAT_SYSCALL_DEFINEx(2, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE3(name, ...) \
	COMPAT_SYSCALL_DEFINEx(3, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE4(name, ...) \
	COMPAT_SYSCALL_DEFINEx(4, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE5(name, ...) \
	COMPAT_SYSCALL_DEFINEx(5, _##name, __VA_ARGS__)
#define COMPAT_SYSCALL_DEFINE6(name, ...) \
	COMPAT_SYSCALL_DEFINEx(6, _##name, __VA_ARGS__)

#define COMPAT_SYSCALL_DEFINEx(x, name, ...)				\
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))\
		__attribute__((alias(__stringify(compat_SyS##name))));  \
	ALLOW_ERROR_INJECTION(compat_sys##name, ERRNO);	\
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))\
	{								\
		return C_SYSC##name(__MAP(x,__SC_DELOUSE,__VA_ARGS__));	\
	}								\
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))

#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
```

---

## 为什么这个改动有效

### 预处理流程

```
编译 fs/mount.c 时：
1. 包含 <linux/syscalls.h>
   ├─ 定义基础宏 __MAP, __SC_DECL 等 ✓
   ├─ #ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
   │  └─ #include <asm/syscall_wrapper.h>
   │     └─ 定义 COMPAT_SYSCALL_DEFINEx (新) ✓
   └─ #endif

2. 包含 <linux/compat.h> (来自某个文件)
   ├─ #ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
   │  └─ COMPAT_SYSCALL_DEFINEx 定义被跳过（因为已启用新框架）✓
   └─ #endif

结果：只有新宏被使用，无重定义，无冲突！
```

### 为什么不在其他文件中改

- **include/linux/syscalls.h**：已正确设置了 include 顺序和条件编译，无需改
- **arch/arm64/include/asm/syscall_wrapper.h**：新宏定义正确，无需改
- **其他文件**：都不定义 COMPAT_SYSCALL_DEFINE*

---

## 验证方法

### 编译前检查
```bash
# 确认修改已应用
grep -n "CONFIG_ARCH_HAS_SYSCALL_WRAPPER" include/linux/compat.h | wc -l  # 应为 2
```

### 编译验证
```bash
cd /home/wjy1214/Desktop/crux-marisa
make -j$(nproc) 2>&1 | grep -i "macro redefined\|COMPAT_SYSCALL"  # 应为空
```

### 链接后验证
```bash
# 检查符号生成
nm vmlinux | grep -E "sys_mount|__arm64_sys_mount" | head -3

# 应该看到类似：
# 0000000000xxxxxx T __arm64_sys_mount
# 0000000000xxxxxx T sys_mount (可选别名)
```

### 运行时验证
```bash
# 系统调用能否正常工作
strace -e mount mount -t tmpfs /tmp/test /test 2>&1 | head -5
```

---

## 文件清单总结

### 必须修改（1 个文件）
- ✅ `include/linux/compat.h` — 条件编译旧宏定义

### 应该验证（3 个文件）
- 📋 `include/linux/syscalls.h` — 确认基础宏完整
- 📋 `arch/arm64/include/asm/syscall_wrapper.h` — 确认新宏完整
- 📋 `.config` — 确认 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y`

### 可选改进（2 个文件）
- 📝 `arch/arm64/kernel/sys_ni.c` — 使用新宏 `COND_SYSCALL()`
- 📝 `arch/arm64/kernel/syscall_64.S` — 确认无手写的 sys_* 引用

### 无需改（3+ 个文件）
- 📌 `arch/arm64/include/uapi/asm/unistd.h` — 系统调用号表
- 📌 系统调用表 — 符号映射由链接器处理
- 📌 其他架构文件

---

## 参考文档

本分析附带以下文档（已生成在仓库目录中）：

1. **SYSCALL_QUICK_FIX.md** — 30秒快速修复指南
2. **SYSCALL_MACRO_ANALYSIS.md** — 深度技术分析
3. **SYSCALL_UPDATE_GUIDE.md** — 详细实施步骤
4. **SYSCALL_DEPENDENCY_MAP.md** — 完整依赖关系图

---

## 最终答案

**问：哪些文件必须成套更新？**

**答：** 从技术上讲，**只有 3 个文件定义 SYSCALL 宏**，你需要确保它们在版本上一致：

| 文件 | 用途 | 何时更新 |
|------|------|--------|
| `include/linux/syscalls.h` | 基础宏 + fallback 旧宏 | ✅ 已正确（无需改） |
| `arch/arm64/include/asm/syscall_wrapper.h` | arch 特定新宏 | ✅ 已正确（无需改） |
| `include/linux/compat.h` | compat 旧宏 | ❌ **需要修改**（条件编译） |

**修改策略**：用 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 条件编译 `include/linux/compat.h` 中的旧宏定义，这样当 arm64 启用新框架时，旧宏会被跳过，由新宏接管。

**预期成果**：一行改动解决所有问题。

