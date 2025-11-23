# SYSCALL 宏体系统一更新 - 快速参考

## TL;DR - 30秒总结

你的内核混用了**两个版本的 SYSCALL 框架**，导致宏冲突和函数未生成。

**解决方案**：一行改动
```bash
# 编辑 include/linux/compat.h
# 在第 33 行（COMPAT_SYSCALL_DEFINE0 前）加入：
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER

# 在第 60 行（所有 COMPAT_SYSCALL_DEFINE* 后）加入：
#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
```

## 为什么会冲突？

| 文件 | 定义的宏 | 适用条件 | 问题 |
|------|--------|--------|------|
| `include/linux/compat.h` | `COMPAT_SYSCALL_DEFINE*` | 总是定义 | 旧版本，使用 alias |
| `arch/arm64/include/asm/syscall_wrapper.h` | `COMPAT_SYSCALL_DEFINE*` | 当 CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y | 新版本，使用 pt_regs |
| 结果 | 两个宏同名 | arm64 启用了新框架 | **重定义警告 + 符号混乱** |

## 核心区别

### 旧宏（include/linux/compat.h）
```c
// 生成函数：compat_sys_mount (别名 -> compat_SyS_mount)
// 接收：普通参数列表
#define COMPAT_SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long compat_sys##name(...)
	__attribute__((alias(__stringify(compat_SyS##name))));
	...
```

### 新宏（arch/arm64/include/asm/syscall_wrapper.h）
```c
// 生成函数：__arm64_compat_sys_mount
// 接收：const struct pt_regs *regs
#define COMPAT_SYSCALL_DEFINEx(x, name, ...)
	asmlinkage long __arm64_compat_sys##name(const struct pt_regs *regs);
	...
```

## 依赖关系（简图）

```
include/linux/syscalls.h
    ├─ 定义基础宏：__MAP, __SC_DECL, __SC_LONG, ...
    ├─ #ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
    │  └─ #include <asm/syscall_wrapper.h>  ← 新宏
    └─ #ifndef __SYSCALL_DEFINEx
       └─ 定义 fallback（旧宏）

include/linux/compat.h
    ├─ 定义 COMPAT_SYSCALL_DEFINE0/1-6（旧宏）← ⚠️ 这里有问题
    └─ 定义 COMPAT_SYSCALL_DEFINEx（旧宏）
```

**问题**：`include/linux/compat.h` 的宏无条件定义，当 arm64 启用新框架时会覆盖。

**解决**：用 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER` 条件编译。

## 修改步骤（2 分钟）

### 1. 打开文件
```bash
vim /home/wjy1214/Desktop/crux-marisa/include/linux/compat.h
```

### 2. 找到第 33 行
```c
#define COMPAT_SYSCALL_DEFINE0(name) \
```

### 3. 在第 33 行前插入一行
```c
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
```

### 4. 找到第 60 行（现在是 61 行）
```c
#define COMPAT_SYSCALL_DEFINEx(x, name, ...)...
	...
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

这一整个宏定义块之后，插入
```c
#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
```

### 5. 保存文件

## 验证修改（1 分钟）

```bash
# 确保宏被条件编译
grep -n "CONFIG_ARCH_HAS_SYSCALL_WRAPPER" include/linux/compat.h  # 应看到 2 行

# 确保没有语法错误
gcc -E -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -Iinclude -Iarch/arm64/include \
    include/linux/compat.h 2>&1 | tail -5  # 应无错误
```

## 编译测试（10 分钟）

```bash
cd /home/wjy1214/Desktop/crux-marisa

# 清空旧编译
make clean

# 重新编译
make -j$(nproc) 2>&1 | tee build.log

# 检查结果
grep -i "macro redefined\|implicit declaration" build.log  # 应为空
```

## 预期成果

修改前：
```
warning: 'COMPAT_SYSCALL_DEFINEx' macro redefined
error: implicit declaration of function 'sys_mount'
```

修改后：
```
✓ 无重定义警告
✓ 所有 sys_* 函数正常生成
✓ 所有 compat_sys_* 函数正常生成
```

## 常见问题

**Q1: 为什么只需要改这一个文件？**

A: 因为其他文件已经正确设置了条件编译逻辑：
- `include/linux/syscalls.h` 已有 `#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER`
- `arch/arm64/include/asm/syscall_wrapper.h` 已提供新宏
- 只是 `include/linux/compat.h` 忽略了这个条件

**Q2: 条件编译会对性能有影响吗？**

A: 没有。这是预处理阶段的操作，编译时就被处理掉了。

**Q3: 如果改了之后编译还是失败怎么办？**

A: 检查编译日志中是否仍有 "macro redefined" 或其他错误。常见原因：
- compat.h 的条件编译没正确关闭（检查 `#endif` 是否匹配）
- CONFIG_ARCH_HAS_SYSCALL_WRAPPER 没有被正确传给编译器

**Q4: 这个改动是永久的吗？**

A: 是的。这是长期支持的配置，与 Linux 主线一致（>=4.17）。

## 文件对照

### 修改前

```c
/* include/linux/compat.h, line 33 */
#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	...

#define COMPAT_SYSCALL_DEFINE1(name, ...) \
	...

#define COMPAT_SYSCALL_DEFINEx(x, name, ...) \
	...
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
```

### 修改后

```c
/* include/linux/compat.h, line 33 */
#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER

#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	...

#define COMPAT_SYSCALL_DEFINE1(name, ...) \
	...

#define COMPAT_SYSCALL_DEFINEx(x, name, ...) \
	...
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))

#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
```

## 更多资源

- **详细分析**：`SYSCALL_MACRO_ANALYSIS.md`
- **实施指南**：`SYSCALL_UPDATE_GUIDE.md`
- **依赖关系图**：`SYSCALL_DEPENDENCY_MAP.md`

## 快速命令参考

```bash
# 查看当前编译状态
cat /home/wjy1214/Desktop/crux-marisa/build.log | grep -i "macro\|implicit"

# 检查 CONFIG_ARCH_HAS_SYSCALL_WRAPPER 是否启用
grep CONFIG_ARCH_HAS_SYSCALL_WRAPPER /home/wjy1214/Desktop/crux-marisa/.config

# 查看修改后的文件
diff <(git show HEAD:include/linux/compat.h) include/linux/compat.h

# 验证符号生成
nm vmlinux | grep sys_mount | head -5

# 运行时检查
strace -e mount ls / 2>&1 | head -10
```

