/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_COMPAT_H
#define _LINUX_COMPAT_H
/*
 * These are the type definitions for the architecture specific
 * syscall compatibility layer.
 */

#include <linux/types.h>

#ifdef CONFIG_COMPAT
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_COMPAT_H
#define _LINUX_COMPAT_H
/*
 * These are the type definitions for the architecture specific
 * syscall compatibility layer.
 */

#include <linux/types.h>

#ifdef CONFIG_COMPAT

#include <linux/stat.h>
#include <linux/param.h>	/* for HZ */
#include <linux/sem.h>
#include <linux/socket.h>
#include <linux/if.h>
#include <linux/fs.h>
#include <linux/aio_abi.h>	/* for aio_context_t */
#include <linux/uaccess.h>
#include <linux/unistd.h>

#include <asm/compat.h>
#include <asm/siginfo.h>
#include <asm/signal.h>

#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
/*
 * It may be useful for an architecture to override the definitions of the
 * COMPAT_SYSCALL_DEFINE0 and COMPAT_SYSCALL_DEFINEx() macros, in particular
 * to use a different calling convention for syscalls. To allow for that,
 * the prototypes for the compat_sys_*() functions below will *not* be included
 * if CONFIG_ARCH_HAS_SYSCALL_WRAPPER is enabled.
 */
#include <asm/syscall_wrapper.h>
#endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */

#ifndef COMPAT_USE_64BIT_TIME
#define COMPAT_USE_64BIT_TIME 0
#endif

#ifndef __SC_DELOUSE
#define __SC_DELOUSE(t,v) ((__force t)(unsigned long)(v))
#endif

#ifndef COMPAT_SYSCALL_DEFINE0
#define COMPAT_SYSCALL_DEFINE0(name) \
	asmlinkage long compat_sys_##name(void); \
	ALLOW_ERROR_INJECTION(compat_sys_##name, ERRNO); \
	asmlinkage long compat_sys_##name(void)
#endif /* COMPAT_SYSCALL_DEFINE0 */

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

#ifndef COMPAT_SYSCALL_DEFINEx
#define COMPAT_SYSCALL_DEFINEx(x, name, ...)                \
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_sys##name(__MAP(x,__SC_DECL,__VA_ARGS__))\
		__attribute__((alias(__stringify(compat_SyS##name))));  \
	ALLOW_ERROR_INJECTION(compat_sys##name, ERRNO);    \
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__));\
	asmlinkage long compat_SyS##name(__MAP(x,__SC_LONG,__VA_ARGS__))\
	{                                \
		return C_SYSC##name(__MAP(x,__SC_DELOUSE,__VA_ARGS__));    \
	}                                \
	static inline long C_SYSC##name(__MAP(x,__SC_DECL,__VA_ARGS__))
#endif /* COMPAT_SYSCALL_DEFINEx */

#ifndef compat_user_stack_pointer
#define compat_user_stack_pointer() current_user_stack_pointer()
#endif
#ifndef compat_sigaltstack	/* we'll need that for MIPS */
typedef struct compat_sigaltstack {
	compat_uptr_t            ss_sp;
	int                ss_flags;
	compat_size_t            ss_size;
} compat_stack_t;
#endif
#ifndef COMPAT_MINSIGSTKSZ
#define COMPAT_MINSIGSTKSZ    MINSIGSTKSZ
#endif

#define compat_jiffies_to_clock_t(x)    \
		(((unsigned long)(x) * COMPAT_USER_HZ) / HZ)

typedef __compat_uid32_t    compat_uid_t;
typedef __compat_gid32_t    compat_gid_t;

typedef    compat_ulong_t        compat_aio_context_t;

struct compat_sel_arg_struct;
struct rusage;

struct compat_itimerspec {
	struct compat_timespec it_interval;
	struct compat_timespec it_value;
};

struct compat_utimbuf {
	compat_time_t        actime;
	compat_time_t        modtime;
};

struct compat_itimerval {
	struct compat_timeval    it_interval;
	struct compat_timeval    it_value;
};

struct itimerval;
int get_compat_itimerval(struct itimerval *, const struct compat_itimerval __user *);
int put_compat_itimerval(struct compat_itimerval __user *, const struct itimerval *);

struct compat_tms {
	compat_clock_t        tms_utime;
	compat_clock_t        tms_stime;
	compat_clock_t        tms_cutime;
	compat_clock_t        tms_cstime;
};

struct compat_timex {
	compat_uint_t modes;
	compat_long_t offset;
	compat_long_t freq;
	compat_long_t maxerror;
	compat_long_t esterror;
	compat_int_t status;
	compat_long_t constant;
	compat_long_t precision;
	compat_long_t tolerance;
	struct compat_timeval time;
	compat_long_t tick;
	compat_long_t ppsfreq;
	compat_long_t jitter;
	compat_int_t shift;
	compat_long_t stabil;
	compat_long_t jitcnt;
	compat_long_t calcnt;
	compat_long_t errcnt;
	compat_long_t stbcnt;
	compat_int_t tai;

	compat_int_t:32; compat_int_t:32; compat_int_t:32; compat_int_t:32;
	compat_int_t:32; compat_int_t:32; compat_int_t:32; compat_int_t:32;
	compat_int_t:32; compat_int_t:32; compat_int_t:32;
};

struct timex;
int compat_get_timex(struct timex *, const struct compat_timex __user *);
int compat_put_timex(struct compat_timex __user *, const struct timex *);

#define _COMPAT_NSIG_WORDS    (_COMPAT_NSIG / _COMPAT_NSIG_BPW)

typedef struct {
	compat_sigset_word    sig[_COMPAT_NSIG_WORDS];
} compat_sigset_t;

struct compat_sigaction {
#ifndef __ARCH_HAS_IRIX_SIGACTION
	compat_uptr_t            sa_handler;
	compat_ulong_t            sa_flags;
#else
	compat_uint_t            sa_flags;
	compat_uptr_t            sa_handler;
#endif
#ifdef __ARCH_HAS_SA_RESTORER
	compat_uptr_t            sa_restorer;
#endif
	compat_sigset_t            sa_mask __packed;
};

typedef union compat_sigval {
	compat_int_t    sival_int;
	compat_uptr_t    sival_ptr;
} compat_sigval_t;

typedef struct compat_siginfo {
	int si_signo;
#ifndef __ARCH_HAS_SWAPPED_SIGINFO
	int si_errno;
	int si_code;
#else
	int si_code;
	int si_errno;
#endif

	union {
		int _pad[128/sizeof(int) - 3];

		/* kill() */
		struct {
			compat_pid_t _pid;    /* sender's pid */
			__compat_uid32_t _uid;    /* sender's uid */
		} _kill;

		/* POSIX.1b timers */
		struct {
			compat_timer_t _tid;    /* timer id */
			int _overrun;        /* overrun count */
			compat_sigval_t _sigval;    /* same as below */
		} _timer;

		/* POSIX.1b signals */
		struct {
			compat_pid_t _pid;    /* sender's pid */
			__compat_uid32_t _uid;    /* sender's uid */
			compat_sigval_t _sigval;
		} _rt;

		/* SIGCHLD */
		struct {
			compat_pid_t _pid;    /* which child */
			__compat_uid32_t _uid;    /* sender's uid */
			int _status;        /* exit code */
			compat_clock_t _utime;
			compat_clock_t _stime;
		} _sigchld;

#ifdef CONFIG_X86_X32_ABI
		/* SIGCHLD (x32 version) */
		struct {
			compat_pid_t _pid;    /* which child */
			__compat_uid32_t _uid;    /* sender's uid */
			int _status;        /* exit code */
			compat_s64 _utime;
			compat_s64 _stime;
		} _sigchld_x32;
#endif

		/* SIGILL, SIGFPE, SIGSEGV, SIGBUS, SIGTRAP, SIGEMT */
		struct {
			compat_uptr_t _addr;    /* faulting insn/memory ref. */
#ifdef __ARCH_SI_TRAPNO
			int _trapno;    /* TRAP # which caused the signal */
#endif
			union {
				/*
				 * used when si_code=BUS_MCEERR_AR or
				 * used when si_code=BUS_MCEERR_AO
				 */
				short int _addr_lsb;    /* Valid LSB of the reported address. */
				/* used when si_code=SEGV_BNDERR */
				struct {
					compat_uptr_t _dummy_bnd;
					compat_uptr_t _lower;
					compat_uptr_t _upper;
				} _addr_bnd;
				/* used when si_code=SEGV_PKUERR */
				struct {
					compat_uptr_t _dummy_pkey;
					u32 _pkey;
				} _addr_pkey;
			};
		} _sigfault;

		/* SIGPOLL */
		struct {
			compat_long_t _band;    /* POLL_IN, POLL_OUT, POLL_MSG */
			int _fd;
		} _sigpoll;

		struct {
			compat_uptr_t _call_addr; /* calling user insn */
			int _syscall;    /* triggering system call number */
			unsigned int _arch;    /* AUDIT_ARCH_* of syscall */
		} _sigsys;
	} _sifields;
} compat_siginfo_t;

/*
 * These functions operate on 32- or 64-bit specs depending on
 * COMPAT_USE_64BIT_TIME, hence the void user pointer arguments.
 */
extern int compat_get_timespec(struct timespec *, const void __user *);
extern int compat_put_timespec(const struct timespec *, void __user *);
extern int compat_get_timeval(struct timeval *, const void __user *);
extern int compat_put_timeval(const struct timeval *, const void __user *);
extern int compat_get_timespec64(struct timespec64 *, const void __user *);
extern int compat_put_timespec64(const struct timespec64 *, const void __user *);
extern int get_compat_itimerspec64(struct itimerspec64 *its,
			const struct compat_itimerspec __user *uits);
extern int put_compat_itimerspec64(const struct itimerspec64 *its,
			struct compat_itimerspec __user *uits);

struct compat_iovec {
	compat_uptr_t    iov_base;
	compat_size_t    iov_len;
};

struct compat_rlimit {
	compat_ulong_t    rlim_cur;
	compat_ulong_t    rlim_max;
};

struct compat_rusage {
	struct compat_timeval ru_utime;
	struct compat_timeval ru_stime;
	compat_long_t    ru_maxrss;
	compat_long_t    ru_ixrss;
	compat_long_t    ru_idrss;
	compat_long_t    ru_isrss;
	compat_long_t    ru_minflt;
	compat_long_t    ru_majflt;
	compat_long_t    ru_nswap;
	compat_long_t    ru_inblock;
	compat_long_t    ru_oublock;
	compat_long_t    ru_msgsnd;
	compat_long_t    ru_msgrcv;
	compat_long_t    ru_nsignals;
	compat_long_t    ru_nvcsw;
	compat_long_t    ru_nivcsw;
};

extern int put_compat_rusage(const struct rusage *,
				 struct compat_rusage __user *);

struct compat_siginfo;

struct compat_dirent {
	u32        d_ino;
	compat_off_t    d_off;
	u16        d_reclen;
	char        d_name[256];
};

struct compat_ustat {
	compat_daddr_t        f_tfree;
	compat_ino_t        f_tinode;
	char            f_fname[6];
	char            f_fpack[6];
};

#define COMPAT_SIGEV_PAD_SIZE    ((SIGEV_MAX_SIZE/sizeof(int)) - 3)

typedef struct compat_sigevent {
	compat_sigval_t sigev_value;
	compat_int_t sigev_signo;
	compat_int_t sigev_notify;
	union {
		compat_int_t _pad[COMPAT_SIGEV_PAD_SIZE];
		compat_int_t _tid;

		struct {
			compat_uptr_t _function;
			compat_uptr_t _attribute;
		} _sigev_thread;
	} _sigev_un;
} compat_sigevent_t;

struct compat_ifmap {
	compat_ulong_t mem_start;
	compat_ulong_t mem_end;
	unsigned short base_addr;
	unsigned char irq;
	unsigned char dma;
	unsigned char port;
};

struct compat_if_settings {
	unsigned int type;    /* Type of physical device or protocol */
	unsigned int size;    /* Size of the data allocated by the caller */
	compat_uptr_t ifs_ifsu;    /* union of pointers */
};

struct compat_ifreq {
	union {
		char    ifrn_name[IFNAMSIZ];    /* if name, e.g. "en0" */
	} ifr_ifrn;
	union {
		struct	sockaddr ifru_addr;
		struct	sockaddr ifru_dstaddr;
		struct	sockaddr ifru_broadaddr;
		struct	sockaddr ifru_netmask;
		struct	sockaddr ifru_hwaddr;
		short	ifru_flags;
		compat_int_t	ifru_ivalue;
		compat_int_t	ifru_mtu;
		struct	compat_ifmap ifru_map;
		char	ifru_slave[IFNAMSIZ];   /* Just fits the size */
		char	ifru_newname[IFNAMSIZ];
		compat_caddr_t	ifru_data;
		struct	compat_if_settings ifru_settings;
	} ifr_ifru;
};

struct compat_ifconf {
	compat_int_t    ifc_len;                /* size of buffer */
	compat_caddr_t  ifcbuf;
};

struct compat_robust_list {
	compat_uptr_t            next;
};

struct compat_robust_list_head {
	struct compat_robust_list    list;
	compat_long_t            futex_offset;
	compat_uptr_t            list_op_pending;
};

#ifdef CONFIG_COMPAT_OLD_SIGACTION
struct compat_old_sigaction {
	compat_uptr_t            sa_handler;
	compat_old_sigset_t        sa_mask;
	compat_ulong_t            sa_flags;
	compat_uptr_t            sa_restorer;
};
#endif

struct compat_keyctl_kdf_params {
	compat_uptr_t hashname;
	compat_uptr_t otherinfo;
	__u32 otherinfolen;
	__u32 __spare[8];
};

struct compat_statfs;
struct compat_statfs64;
struct compat_old_linux_dirent;
struct compat_linux_dirent;
struct linux_dirent64;
struct compat_msghdr;
struct compat_mmsghdr;
struct compat_sysinfo;
struct compat_sysctl_args;
struct compat_kexec_segment;
struct compat_mq_attr;
struct compat_msgbuf;

extern void compat_exit_robust_list(struct task_struct *curr);

#define BITS_PER_COMPAT_LONG    (8*sizeof(compat_long_t))

#define BITS_TO_COMPAT_LONGS(bits) DIV_ROUND_UP(bits, BITS_PER_COMPAT_LONG)

long compat_get_bitmap(unsigned long *mask, const compat_ulong_t __user *umask,
			   unsigned long bitmap_size);
long compat_put_bitmap(compat_ulong_t __user *umask, unsigned long *mask,
			   unsigned long bitmap_size);
int copy_siginfo_from_user32(siginfo_t *to, const struct compat_siginfo __user *from);
int copy_siginfo_to_user32(struct compat_siginfo __user *to, const siginfo_t *from);
int get_compat_sigevent(struct sigevent *event,
		const struct compat_sigevent __user *u_event);

static inline int compat_timeval_compare(struct compat_timeval *lhs,
					struct compat_timeval *rhs)
{
	if (lhs->tv_sec < rhs->tv_sec)
		return -1;
	if (lhs->tv_sec > rhs->tv_sec)
		return 1;
	return lhs->tv_usec - rhs->tv_usec;
}

static inline int compat_timespec_compare(struct compat_timespec *lhs,
					struct compat_timespec *rhs)
{
	if (lhs->tv_sec < rhs->tv_sec)
		return -1;
	if (lhs->tv_sec > rhs->tv_sec)
		return 1;
	return lhs->tv_nsec - rhs->tv_nsec;
}

extern int get_compat_itimerspec(struct itimerspec *dst,
				 const struct compat_itimerspec __user *src);
extern int put_compat_itimerspec(struct compat_itimerspec __user *dst,
				 const struct itimerspec *src);

extern void sigset_from_compat(sigset_t *set, const compat_sigset_t *compat);
extern void sigset_to_compat(compat_sigset_t *compat, const sigset_t *set);

extern int compat_ptrace_request(struct task_struct *child,
				 compat_long_t request,
				 compat_ulong_t addr, compat_ulong_t data);

extern long compat_arch_ptrace(struct task_struct *child, compat_long_t request,
				   compat_ulong_t addr, compat_ulong_t data);

extern ssize_t compat_rw_copy_check_uvector(int type,
		const struct compat_iovec __user *uvector,
		unsigned long nr_segs,
		unsigned long fast_segs, struct iovec *fast_pointer,
		struct iovec **ret_pointer);

extern void __user *compat_alloc_user_space(unsigned long len);

asmlinkage ssize_t compat_sys_process_vm_readv(compat_pid_t pid,
		const struct compat_iovec __user *lvec,
		compat_ulong_t liovcnt, const struct compat_iovec __user *rvec,
		compat_ulong_t riovcnt, compat_ulong_t flags);
asmlinkage ssize_t compat_sys_process_vm_writev(compat_pid_t pid,
		const struct compat_iovec __user *lvec,
		compat_ulong_t liovcnt, const struct compat_iovec __user *rvec,
		compat_ulong_t riovcnt, compat_ulong_t flags);

asmlinkage long compat_sys_sendfile(int out_fd, int in_fd,
					compat_off_t __user *offset, compat_size_t count);
asmlinkage long compat_sys_sendfile64(int out_fd, int in_fd,
					compat_loff_t __user *offset, compat_size_t count);
asmlinkage long compat_sys_sigaltstack(const compat_stack_t __user *uss_ptr,
					   compat_stack_t __user *uoss_ptr);

#ifdef __ARCH_WANT_SYS_SIGPENDING
asmlinkage long compat_sys_sigpending(compat_old_sigset_t __user *set);
#endif

#ifdef __ARCH_WANT_SYS_SIGPROCMASK
asmlinkage long compat_sys_sigprocmask(int how, compat_old_sigset_t __user *nset,
					   compat_old_sigset_t __user *oset);
#endif

int compat_restore_altstack(const compat_stack_t __user *uss);
int __compat_save_altstack(compat_stack_t __user *, unsigned long);
#define compat_save_altstack_ex(uss, sp) do { \
	compat_stack_t __user *__uss = uss; \
	struct task_struct *t = current; \
	put_user_ex(ptr_to_compat((void __user *)t->sas_ss_sp), &__uss->ss_sp); \
	put_user_ex(t->sas_ss_flags, &__uss->ss_flags); \
	put_user_ex(t->sas_ss_size, &__uss->ss_size); \
	if (t->sas_ss_flags & SS_AUTODISARM) \
		sas_ss_reset(t); \
} while (0);

asmlinkage long compat_sys_sched_rr_get_interval(compat_pid_t pid,
						 struct compat_timespec __user *interval);

asmlinkage long compat_sys_fanotify_mark(int, unsigned int, __u32, __u32,
						int, const char __user *);

/*
 * For most but not all architectures, "am I in a compat syscall?" and
 * "am I a compat task?" are the same question.  For architectures on which
 * they aren't the same question, arch code can override in_compat_syscall.
 */

#ifndef in_compat_syscall
static inline bool in_compat_syscall(void) { return is_compat_task(); }
#endif

/**
 * ns_to_compat_timeval - Compat version of ns_to_timeval
 * @nsec:    the nanoseconds value to be converted
 *
 * Returns the compat_timeval representation of the nsec parameter.
 */
static inline struct compat_timeval ns_to_compat_timeval(s64 nsec)
{
	struct timeval tv;
	struct compat_timeval ctv;

	tv = ns_to_timeval(nsec);
	ctv.tv_sec = tv.tv_sec;
	ctv.tv_usec = tv.tv_usec;

	return ctv;
}

/*
 * Kernel code should not call compat syscalls (i.e., compat_sys_xyzyyz())
 * directly.  Instead, use one of the functions which work equivalently, such
 * as the kcompat_sys_xyzyyz() functions prototyped below.
 */
int kcompat_sys_statfs64(const char __user * pathname, compat_size_t sz,
			 struct compat_statfs64 __user * buf);
int kcompat_sys_fstatfs64(unsigned int fd, compat_size_t sz,
			  struct compat_statfs64 __user * buf);

#else /* !CONFIG_COMPAT */

#define is_compat_task() (0)
static inline bool in_compat_syscall(void) { return false; }

#endif /* CONFIG_COMPAT */

#endif /* _LINUX_COMPAT_H */
	if (lhs->tv_sec > rhs->tv_sec)
		return 1;
	return lhs->tv_usec - rhs->tv_usec;
}

static inline int compat_timespec_compare(struct compat_timespec *lhs,
					struct compat_timespec *rhs)
{
	if (lhs->tv_sec < rhs->tv_sec)
		return -1;
	if (lhs->tv_sec > rhs->tv_sec)
		return 1;
	return lhs->tv_nsec - rhs->tv_nsec;
}

extern int get_compat_itimerspec(struct itimerspec *dst,
				 const struct compat_itimerspec __user *src);
extern int put_compat_itimerspec(struct compat_itimerspec __user *dst,
				 const struct itimerspec *src);

extern void sigset_from_compat(sigset_t *set, const compat_sigset_t *compat);
extern void sigset_to_compat(compat_sigset_t *compat, const sigset_t *set);

extern int compat_ptrace_request(struct task_struct *child,
				 compat_long_t request,
				 compat_ulong_t addr, compat_ulong_t data);

extern long compat_arch_ptrace(struct task_struct *child, compat_long_t request,
			       compat_ulong_t addr, compat_ulong_t data);

extern ssize_t compat_rw_copy_check_uvector(int type,
		const struct compat_iovec __user *uvector,
		unsigned long nr_segs,
		unsigned long fast_segs, struct iovec *fast_pointer,
		struct iovec **ret_pointer);

extern void __user *compat_alloc_user_space(unsigned long len);

asmlinkage ssize_t compat_sys_process_vm_readv(compat_pid_t pid,
		const struct compat_iovec __user *lvec,
		compat_ulong_t liovcnt, const struct compat_iovec __user *rvec,
		compat_ulong_t riovcnt, compat_ulong_t flags);
asmlinkage ssize_t compat_sys_process_vm_writev(compat_pid_t pid,
		const struct compat_iovec __user *lvec,
		compat_ulong_t liovcnt, const struct compat_iovec __user *rvec,
		compat_ulong_t riovcnt, compat_ulong_t flags);

asmlinkage long compat_sys_sendfile(int out_fd, int in_fd,
				    compat_off_t __user *offset, compat_size_t count);
asmlinkage long compat_sys_sendfile64(int out_fd, int in_fd,
				    compat_loff_t __user *offset, compat_size_t count);
asmlinkage long compat_sys_sigaltstack(const compat_stack_t __user *uss_ptr,
				       compat_stack_t __user *uoss_ptr);

#ifdef __ARCH_WANT_SYS_SIGPENDING
asmlinkage long compat_sys_sigpending(compat_old_sigset_t __user *set);
#endif

#ifdef __ARCH_WANT_SYS_SIGPROCMASK
asmlinkage long compat_sys_sigprocmask(int how, compat_old_sigset_t __user *nset,
				       compat_old_sigset_t __user *oset);
#endif

int compat_restore_altstack(const compat_stack_t __user *uss);
int __compat_save_altstack(compat_stack_t __user *, unsigned long);
#define compat_save_altstack_ex(uss, sp) do { \
	compat_stack_t __user *__uss = uss; \
	struct task_struct *t = current; \
	put_user_ex(ptr_to_compat((void __user *)t->sas_ss_sp), &__uss->ss_sp); \
	put_user_ex(t->sas_ss_flags, &__uss->ss_flags); \
	put_user_ex(t->sas_ss_size, &__uss->ss_size); \
	if (t->sas_ss_flags & SS_AUTODISARM) \
		sas_ss_reset(t); \
} while (0);

asmlinkage long compat_sys_sched_rr_get_interval(compat_pid_t pid,
						 struct compat_timespec __user *interval);

asmlinkage long compat_sys_fanotify_mark(int, unsigned int, __u32, __u32,
					    int, const char __user *);

/*
 * For most but not all architectures, "am I in a compat syscall?" and
 * "am I a compat task?" are the same question.  For architectures on which
 * they aren't the same question, arch code can override in_compat_syscall.
 */

#ifndef in_compat_syscall
static inline bool in_compat_syscall(void) { return is_compat_task(); }
#endif

/**
 * ns_to_compat_timeval - Compat version of ns_to_timeval
 * @nsec:	the nanoseconds value to be converted
 *
 * Returns the compat_timeval representation of the nsec parameter.
 */
static inline struct compat_timeval ns_to_compat_timeval(s64 nsec)
{
	struct timeval tv;
	struct compat_timeval ctv;

	tv = ns_to_timeval(nsec);
	ctv.tv_sec = tv.tv_sec;
	ctv.tv_usec = tv.tv_usec;

	return ctv;
}

/*
 * Kernel code should not call compat syscalls (i.e., compat_sys_xyzyyz())
 * directly.  Instead, use one of the functions which work equivalently, such
 * as the kcompat_sys_xyzyyz() functions prototyped below.
 */

int kcompat_sys_statfs64(const char __user * pathname, compat_size_t sz,
		     struct compat_statfs64 __user * buf);
int kcompat_sys_fstatfs64(unsigned int fd, compat_size_t sz,
			  struct compat_statfs64 __user * buf);

#else /* !CONFIG_COMPAT */

#define is_compat_task() (0)
static inline bool in_compat_syscall(void) { return false; }

#endif /* CONFIG_COMPAT */

#endif /* _LINUX_COMPAT_H */
