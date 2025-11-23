# SYSCALL 宏体系统一更新方案（实施指南）

## 方案选择：采纳新框架（CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y）

### 涉及的文件清单

| 优先级 | 文件路径 | 操作 | 风险 | 验证方法 |
|-------|--------|------|------|--------|
| 🔴 必须 | `include/linux/compat.h` | 条件编译旧宏 | 低 | grep COMPAT_SYSCALL_DEFINE |
| 🟡 应该 | `include/linux/syscalls.h` | 验证基础宏完整 | 低 | grep __MAP/__SC_DECL |
| 🟡 应该 | `arch/arm64/include/asm/syscall_wrapper.h` | 验证新宏完整 | 低 | grep COND_SYSCALL |
| 🟢 可选 | `arch/arm64/kernel/sys_ni.c` | 使用新宏 | 中 | nm vmlinux \| grep sys_ni |
| 🟢 可选 | `arch/arm64/kernel/syscall_64.S` | 检查符号引用 | 中 | objdump -t |

---

## 详细改动说明

### 1. include/linux/compat.h（🔴 必须）

#### 当前问题
```
第 34-60 行：COMPAT_SYSCALL_DEFINE0/1-6 和 COMPAT_SYSCALL_DEFINEx 使用旧语法
```

#### 正确做法

**在所有 COMPAT_SYSCALL_DEFINE* 定义前添加条件**：

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

#### 为什么这样做
- arm64 启用 `CONFIG_ARCH_HAS_SYSCALL_WRAPPER=y`
- 此时 `arch/arm64/include/asm/syscall_wrapper.h` 会被 `include/linux/syscalls.h` 包含
- syscall_wrapper.h 中已定义了新版本的 COMPAT_SYSCALL_DEFINE*
- 旧定义被条件编译跳过，避免重定义警告

#### 验证
```bash
# 编译时不出现 "macro redefined" 警告
make -C /home/wjy1214/Desktop/crux-marisa 2>&1 | grep -i "COMPAT_SYSCALL.*redefined"  # 应为空

# 检查条件编译是否正确
gcc -E -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -Iinclude -Iarch/arm64/include \
    include/linux/compat.h 2>&1 | grep -c "COMPAT_SYSCALL_DEFINE"  # 应为 0（被跳过）
```

---

### 2. include/linux/syscalls.h（🟡 应该）

#### 当前状态检查

```bash
# 检查基础宏是否完整
grep -n "^#define __MAP" include/linux/syscalls.h      # 应有 __MAP0-6
grep -n "^#define __SC_DECL" include/linux/syscalls.h  # 应有 __SC_DECL
grep -n "^#define __SC_LONG" include/linux/syscalls.h  # 应有 __SC_LONG
grep -n "^#define __SC_CAST" include/linux/syscalls.h  # 应有 __SC_CAST
grep -n "^#define __SC_TEST" include/linux/syscalls.h  # 应有 __SC_TEST
grep -n "^#define __SC_ARGS" include/linux/syscalls.h  # 应有 __SC_ARGS
grep -n "^#define __PROTECT" include/linux/syscalls.h  # 应有 __PROTECT
```

#### 正确结构（现有代码应已正确，确认即可）

```c
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
#include <asm/syscall_wrapper.h>
#endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */

/* 基础宏始终定义 */
#define __MAP0(m,...)
#define __MAP1(m,t,a,...) m(t,a)
#define __MAP2(m,t,a,...) m(t,a), __MAP1(m,__VA_ARGS__)
...
#define __SC_DECL(t, a)	t a
#define __SC_LONG(t, a) __typeof(__builtin_choose_expr(__TYPE_IS_LL(t), 0LL, 0L)) a
#define __SC_CAST(t, a)	(__force t) a
#define __SC_TEST(t, a) (void)BUILD_BUG_ON_ZERO(...)
#define __SC_ARGS(t, a)	a
#define __PROTECT(...) asmlinkage_protect(__VA_ARGS__)

/* SYSCALL_METADATA 定义（ftrace 相关） */
#ifdef CONFIG_FTRACE_SYSCALLS
#define SYSCALL_METADATA(sname, nb, ...) ...
#else
#define SYSCALL_METADATA(sname, nb, ...)
#endif

/* fallback 宏定义，仅在 !CONFIG_ARCH_HAS_SYSCALL_WRAPPER 时使用 */
#ifndef __SYSCALL_DEFINEx
#define __SYSCALL_DEFINEx(x, name, ...) ...
#endif
```

#### 验证
```bash
# 确保 arm64 使用的是新版本（来自 syscall_wrapper.h）
echo | gcc -E -DCONFIG_ARCH_HAS_SYSCALL_WRAPPER \
    -DCONFIG_ARCH_ARM64 \
    -Iinclude -Iarch/arm64/include \
    -x c -dM - | grep "__SYSCALL_DEFINEx" | head -1  # 应显示来自 syscall_wrapper.h 的定义
```

---

### 3. arch/arm64/include/asm/syscall_wrapper.h（🟡 应该）

#### 检查清单

**已有宏定义**：
```bash
grep -n "^#define SYSCALL_DEFINE0" arch/arm64/include/asm/syscall_wrapper.h
grep -n "^#define __SYSCALL_DEFINEx" arch/arm64/include/asm/syscall_wrapper.h
grep -n "^#define COMPAT_SYSCALL_DEFINE0" arch/arm64/include/asm/syscall_wrapper.h
grep -n "^#define COMPAT_SYSCALL_DEFINEx" arch/arm64/include/asm/syscall_wrapper.h
grep -n "^#define COND_SYSCALL" arch/arm64/include/asm/syscall_wrapper.h
grep -n "^#define SYS_NI" arch/arm64/include/asm/syscall_wrapper.h
```

**应该全部存在，已验证文件中确实包含这些定义** ✓

#### 关键特性验证
```bash
# 1. 确保宏接收 pt_regs
grep -A2 "^#define __SYSCALL_DEFINEx" arch/arm64/include/asm/syscall_wrapper.h | \
  grep -q "const struct pt_regs"  # 应返回 0（即找到）

# 2. 确保 SC_ARM64_REGS_TO_ARGS 定义
grep -n "SC_ARM64_REGS_TO_ARGS" arch/arm64/include/asm/syscall_wrapper.h  # 应找到

# 3. 确保使用 __MAP、__SC_* 宏
grep -q "__MAP.*__SC_ARGS.*regs->regs" arch/arm64/include/asm/syscall_wrapper.h  # 应返回 0
```

**现有文件已正确** ✓

---

### 4. arch/arm64/kernel/sys_ni.c（🟢 可选，但推荐更新）

#### 检查现状
```bash
grep "COND_SYSCALL\|SYS_NI" arch/arm64/kernel/sys_ni.c
```

#### 正确用法

**旧用法**（不再使用）：
```c
cond_syscall(sys_foo);       // 通用 cond_syscall
cond_syscall(compat_sys_bar);
```

**新用法**（推荐）：
```c
COND_SYSCALL(foo);           // 自动转换为 __arm64_sys_foo
#ifdef CONFIG_COMPAT
COND_SYSCALL_COMPAT(bar);    // 自动转换为 __arm64_compat_sys_bar
#endif
```

#### 操作（如果 sys_ni.c 仍使用旧宏）

假设 sys_ni.c 内容类似：
```c
cond_syscall(sys_foo);
cond_syscall(compat_sys_bar);
```

应改为：
```c
COND_SYSCALL(foo);
#ifdef CONFIG_COMPAT
COND_SYSCALL_COMPAT(bar);
#endif
```

#### 验证
```bash
# 编译后检查符号是否正确生成
nm vmlinux | grep -E "__arm64_sys_ni|__arm64_compat_sys_ni"  # 应能找到
```

---

### 5. 系统调用表和符号映射（🟢 可选，通常无需改）

#### 检查位置

```bash
# arm64 系统调用表
cat arch/arm64/include/uapi/asm/unistd.h | head -30

# 符号引用
grep -r "sys_mount\|__arm64_sys_mount" arch/arm64/kernel/
```

#### 期望状态

- 系统调用表仍引用 `sys_*` 符号名
- 链接器通过 SYSCALL_ALIAS 或其他机制找到 `__arm64_sys_*` 实现
- 无需手动改表

#### 验证
```bash
# 最终 vmlinux 中符号应该存在
nm vmlinux | grep " sys_mount"       # 可能是 __arm64_sys_mount 的别名
nm vmlinux | grep " __arm64_sys_mount"  # 应该存在实现
```

---

## 实施步骤（总结）

### Phase 1：修改（15 分钟）

1. **编辑** `include/linux/compat.h`
   - 在第 33 行（当前 COMPAT_SYSCALL_DEFINE0 定义之前）加入 `#ifndef CONFIG_ARCH_HAS_SYSCALL_WRAPPER`
   - 在所有 COMPAT_SYSCALL_DEFINE* 定义之后加入 `#endif /* !CONFIG_ARCH_HAS_SYSCALL_WRAPPER */`
   
2. **验证** `include/linux/syscalls.h`
   - 确保所有基础宏完整（通常已有）
   
3. **验证** `arch/arm64/include/asm/syscall_wrapper.h`
   - 确保所有新宏完整（通常已有）

### Phase 2：编译验证（10 分钟）

```bash
cd /home/wjy1214/Desktop/crux-marisa

# 清空编译产物
make clean
make mrproper

# 重新编译
make -j$(nproc) 2>&1 | tee build.log.new

# 检查警告
grep -i "macro redefined\|implicit declaration\|undefined reference" build.log.new  # 应为空
```

### Phase 3：运行时验证（5 分钟）

```bash
# 检查符号
nm vmlinux | grep -E "sys_mount|__arm64_sys_mount"

# 查看内核消息
dmesg | head -20

# 简单系统调用测试
echo "test" > /tmp/test.txt
strace -e open,openat,mount,umount ls /tmp/test.txt 2>&1 | head -20
```

---

## 回滚方案

若修改后出现问题，回滚如下：

```bash
# 恢复 include/linux/compat.h
git checkout include/linux/compat.h

# 清空编译产物并重新编译
make clean
make -j$(nproc)
```

---

## 预期成果

修改完成后：

- ✓ `COMPAT_SYSCALL_DEFINE*` 宏不再出现重定义警告
- ✓ 系统调用 `sys_*` 符号正常生成（作为 `__arm64_sys_*` 的别名或直接生成）
- ✓ `compat_sys_*` 符号正常生成（作为 `__arm64_compat_sys_*` 的别名或直接生成）
- ✓ 编译器不再报 "implicit declaration of function 'sys_mount'" 错误
- ✓ 系统调用能正常执行

