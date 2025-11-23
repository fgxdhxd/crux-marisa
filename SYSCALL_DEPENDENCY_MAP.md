# SYSCALL 宏体系文件依赖关系图

## 包含关系树

```
arch/arm64/kernel/mount.c 等
    ↓ #include <linux/syscalls.h>
include/linux/syscalls.h
    ├─ 定义基础宏（__MAP, __SC_*, __PROTECT）✓
    ├─ 定义 SYSCALL_METADATA
    └─ #ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
       ↓ #include <asm/syscall_wrapper.h>
       arch/arm64/include/asm/syscall_wrapper.h
       ├─ 定义 SYSCALL_DEFINE0 (新版本)
       ├─ 定义 __SYSCALL_DEFINEx (新版本, 接收 pt_regs)
       ├─ #ifdef CONFIG_COMPAT
       │  ├─ 定义 COMPAT_SYSCALL_DEFINE0 (新版本)
       │  ├─ 定义 COMPAT_SYSCALL_DEFINEx (新版本, 接收 pt_regs)
       │  └─ 定义 COND_SYSCALL_COMPAT
       ├─ 定义 COND_SYSCALL
       ├─ 定义 SYS_NI
       └─ 定义 COMPAT_SYS_NI

arch/arm64/kernel/sys_ni.c
    ├─ #include <linux/syscalls.h>  (已上)
    └─ 使用 COND_SYSCALL(), COND_SYSCALL_COMPAT()

arch/arm64/kernel/syscall_64.S
    ├─ 系统调用分发表
    └─ 引用 __arm64_sys_* 符号 (由宏展开生成)

fs/mount.c, kernel/signal.c 等 (使用 SYSCALL_DEFINE*)
    ↓
    #include <linux/syscalls.h>
    ↓
    宏展开生成 __arm64_sys_mount, __arm64_sys_signal 等


include/linux/compat.h ← ⚠️ 需要条件编译
    ├─ 包含 asm/compat.h
    ├─ #ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER (需要添加)
    │  ├─ 定义 COMPAT_SYSCALL_DEFINE0 (旧版本)
    │  ├─ 定义 COMPAT_SYSCALL_DEFINEx (旧版本)
    │  └─ 使用 alias(__stringify(compat_SyS##name))
    └─ #endif (需要添加)
```

## 符号生成流程

### 新框架（CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y）

```
fs/mount.c:
    SYSCALL_DEFINE2(mount, ...)
        ↓ 宏展开（来自 syscall_wrapper.h）
    asmlinkage long __arm64_sys_mount(const struct pt_regs *regs);
    ALLOW_ERROR_INJECTION(__arm64_sys_mount, ERRNO);
    static long __se_sys_mount(...);
    static inline long __do_sys_mount(...);
    asmlinkage long __arm64_sys_mount(const struct pt_regs *regs) {
        return __se_sys_mount(SC_ARM64_REGS_TO_ARGS(2, ...));
    }
    static long __se_sys_mount(...) {
        long ret = __do_sys_mount(__MAP(2,__SC_CAST,...));
        __MAP(2,__SC_TEST,...);
        __PROTECT(2, ret, __MAP(2,__SC_ARGS,...));
        return ret;
    }
    static inline long __do_sys_mount(...) { ... 用户实现 ... }
        ↓ 编译器
    目标文件中的符号：__arm64_sys_mount (实现)

syscall_64.S:
    .quad __arm64_sys_mount    // 系统调用表中的条目
        ↓ 链接器
    vmlinux:
        __arm64_sys_mount       // 实现符号
        sys_mount               // 别名（可选，用于兼容）
```

### 旧框架（CONFIG_ARCH_HAS_SYSCALL_WRAPPER=n，现在被跳过）

```
fs/mount.c:
    SYSCALL_DEFINE2(mount, ...)
        ↓ 宏展开（来自 syscalls.h 的 fallback）
    asmlinkage long sys_mount(...);
    ALLOW_ERROR_INJECTION(sys_mount, ERRNO);
    static inline long SYSC_mount(...);
    asmlinkage long SyS_mount(...);
    asmlinkage long SyS_mount(...) {
        long ret = SYSC_mount(__MAP(2,__SC_CAST,...));
        __MAP(2,__SC_TEST,...);
        __PROTECT(2, ret, __MAP(2,__SC_ARGS,...));
        return ret;
    }
    static inline long SYSC_mount(...) { ... 用户实现 ... }
        ↓ 编译器
    目标文件中的符号：sys_mount (别名), SyS_mount (实现)
```

## 文件修改矩阵

### 必须修改

| 文件 | 行号范围 | 操作 | 原因 |
|------|--------|------|------|
| `include/linux/compat.h` | 33 前 | 加入 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` | 条件编译旧宏 |
| `include/linux/compat.h` | 60 后 | 加入 `#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */` | 闭合条件编译 |

### 应该验证

| 文件 | 检查项 | 期望状态 |
|------|--------|--------|
| `include/linux/syscalls.h` | `__MAP0-6` 定义 | ✓ 存在 |
| `include/linux/syscalls.h` | `__SC_DECL/__SC_LONG` 等 | ✓ 存在 |
| `include/linux/syscalls.h` | `#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` | ✓ 存在 |
| `arch/arm64/include/asm/syscall_wrapper.h` | `__SYSCALL_DEFINEx` 接收 `const struct pt_regs *regs` | ✓ 正确 |
| `arch/arm64/include/asm/syscall_wrapper.h` | `COND_SYSCALL` 宏 | ✓ 存在 |

### 可选改进

| 文件 | 改动 | 优先级 |
|------|------|--------|
| `arch/arm64/kernel/sys_ni.c` | 使用 `COND_SYSCALL()` 代替 `cond_syscall(sys_foo)` | 中 |
| `arch/arm64/kernel/entry.S` | 确保无手写的 `sys_*` 符号引用 | 低 |

## 宏定义查找表

### 基础宏的正确来源

| 宏 | 定义位置 | 何时定义 | 何时使用 |
|---|--------|--------|--------|
| `__MAP`, `__MAP0-6` | `include/linux/syscalls.h` | 总是 | 宏展开参数映射 |
| `__SC_DECL` | `include/linux/syscalls.h` | 总是 | 参数声明 |
| `__SC_LONG` | `include/linux/syscalls.h` | 总是 | 参数类型转换 |
| `__SC_CAST` | `include/linux/syscalls.h` | 总是 | 参数强制转换 |
| `__SC_TEST` | `include/linux/syscalls.h` | 总是 | 编译时检查 |
| `__SC_ARGS` | `include/linux/syscalls.h` | 总是 | 参数提取 |
| `__SC_DELOUSE` | `include/linux/compat.h` | 总是 | compat 类型洗白 |
| `__PROTECT` | `include/linux/syscalls.h` | 总是 | 参数保护 |
| `SYSCALL_METADATA` | `include/linux/syscalls.h` | 当 `CONFIG_FTRACE_SYSCALLS` 时 | 生成 ftrace 元数据 |
| `ALLOW_ERROR_INJECTION` | `include/linux/syscalls.h` 或 fallback | 总是 | 允许错误注入 |
| `SYSCALL_DEFINE0` | `include/linux/syscalls.h` 或 `arch/.../syscall_wrapper.h` | 条件，取决于 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER` | 0 参系统调用 |
| `__SYSCALL_DEFINEx` | `include/linux/syscalls.h` 或 `arch/.../syscall_wrapper.h` | 条件，取决于 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER` | N 参系统调用 |
| `COMPAT_SYSCALL_DEFINE0` | `include/linux/compat.h` 或 `arch/.../syscall_wrapper.h` | 条件，取决于 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 和 `CONFIG_COMPAT` | 0 参 compat 系统调用 |
| `COMPAT_SYSCALL_DEFINEx` | `include/linux/compat.h` 或 `arch/.../syscall_wrapper.h` | 条件，取决于 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 和 `CONFIG_COMPAT` | N 参 compat 系统调用 |
| `COND_SYSCALL` | `include/linux/syscalls.h` 或 `arch/.../syscall_wrapper.h` | 总是 | 条件系统调用 |

## 编译流程检查点

### 预处理阶段（gcc -E）

```bash
# 检查点 1：基础宏已定义
gcc -E -I/home/wjy1214/Desktop/crux-marisa/include \
    -I/home/wjy1214/Desktop/crux-marisa/arch/arm64/include \
    /dev/null 2>&1 | grep "__MAP.*__MAP1"  # 应找到

# 检查点 2：arch 特定宏已包含
gcc -E -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -I/home/wjy1214/Desktop/crux-marisa/include \
    -I/home/wjy1214/Desktop/crux-marisa/arch/arm64/include \
    /dev/null 2>&1 | grep "__SYSCALL_DEFINEx" | head -1  # 应来自 syscall_wrapper.h

# 检查点 3：compat 宏被条件编译
gcc -E -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -DCONFIG_COMPAT \
    -I/home/wjy1214/Desktop/crux-marisa/include \
    -I/home/wjy1214/Desktop/crux-marisa/arch/arm64/include \
    /dev/null 2>&1 | grep "COMPAT_SYSCALL_DEFINE0" | grep -c "asmlinkage long __arm64_compat_sys_"  # 应为 1
```

### 编译阶段（gcc -S）

```bash
# 检查点 4：正确的符号生成
gcc -S -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -I/home/wjy1214/Desktop/crux-marisa/include \
    -I/home/wjy1214/Desktop/crux-marisa/arch/arm64/include \
    /home/wjy1214/Desktop/crux-marisa/fs/mount.c \
    -o /tmp/mount.s 2>&1
grep -E "__arm64_sys_mount|sys_mount" /tmp/mount.s | head -5  # 应找到 __arm64_sys_mount
```

### 链接阶段（nm）

```bash
# 检查点 5：最终符号正确
nm /home/wjy1214/Desktop/crux-marisa/vmlinux | grep mount  # 应找到 __arm64_sys_mount 和/或 sys_mount
```

## 故障排查流程图

```
编译失败？
├─ "macro redefined" 警告
│  ├─ 检查：COMPAT_SYSCALL_DEFINE* 是否被双重定义
│  └─ 解决：在 include/linux/compat.h 中条件编译旧宏
│
├─ "implicit declaration of function 'sys_mount'"
│  ├─ 检查：CONFIG_ARCH_HAS_SYSCALL_WRAPPER 是否启用
│  ├─ 检查：arch/arm64/include/asm/syscall_wrapper.h 是否被正确包含
│  └─ 解决：确保 include 链条完整
│
├─ 系统调用表错误（entry.S 编译失败）
│  ├─ 检查：是否仍在使用旧的 sys_* 符号（应改用 __arm64_sys_*）
│  └─ 解决：更新汇编代码使用新符号
│
└─ 运行时错误（系统调用不工作）
   ├─ 检查：nm vmlinux 是否包含正确的符号
   ├─ 检查：strace 是否能追踪到系统调用
   └─ 解决：重新编译或检查 syscall 表配置
```

