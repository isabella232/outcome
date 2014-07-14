/* spinlock.hpp
Provides yet another spinlock
(C) 2013-2014 Niall Douglas http://www.nedprod.com/
File Created: Sept 2013


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_SPINLOCK_HPP
#define BOOST_SPINLOCK_HPP

#include <cassert>

#if !defined(BOOST_SPINLOCK_USE_BOOST_ATOMIC) && defined(BOOST_NO_CXX11_HDR_ATOMIC)
# define BOOST_SPINLOCK_USE_BOOST_ATOMIC
#endif
#if !defined(BOOST_SPINLOCK_USE_BOOST_THREAD) && defined(BOOST_NO_CXX11_HDR_THREAD)
# define BOOST_SPINLOCK_USE_BOOST_THREAD
#endif

#ifdef BOOST_SPINLOCK_USE_BOOST_ATOMIC
# include <boost/atomic.hpp>
#else
# include <atomic>
#endif
#ifdef BOOST_SPINLOCK_USE_BOOST_THREAD
# include <boost/thread.hpp>
#else
# include <thread>
# include <chrono>
#endif

// Turn this on if you have a compiler which understands __transaction_relaxed
//#define BOOST_HAVE_TRANSACTIONAL_MEMORY_COMPILER

#if !defined(BOOST_COMPILING_FOR_GCOV) && !defined(BOOST_AFIO_COMPILING_FOR_GCOV) // transaction support murders poor old gcov
// Turn this on if you want to use Haswell TSX where available
# if (defined(_MSC_VER) && _MSC_VER >= 1700 && ( defined(_M_IX86) || defined(_M_X64) )) || (defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) ))
#  define BOOST_USING_INTEL_TSX
# endif
#endif

#ifndef BOOST_SMT_PAUSE
# if defined(_MSC_VER) && _MSC_VER >= 1310 && ( defined(_M_IX86) || defined(_M_X64) )
extern "C" void _mm_pause();
#  pragma intrinsic( _mm_pause )
#  define BOOST_SMT_PAUSE _mm_pause();
# elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )
#  define BOOST_SMT_PAUSE __asm__ __volatile__( "rep; nop" : : : "memory" );
# endif
#endif

#ifndef BOOST_NOEXCEPT
# if !defined(_MSC_VER) || _MSC_VER >= 1900
#  define BOOST_NOEXCEPT noexcept
# endif
#endif
#ifndef BOOST_NOEXCEPT
# define BOOST_NOEXCEPT
#endif

#ifndef BOOST_NOEXCEPT_OR_NOTHROW
# if !defined(_MSC_VER) || _MSC_VER >= 1900
#  define BOOST_NOEXCEPT_OR_NOTHROW noexcept
# endif
#endif
#ifndef BOOST_NOEXCEPT_OR_NOTHROW
# define BOOST_NOEXCEPT_OR_NOTHROW throw()
#endif

#ifndef BOOST_CONSTEXPR
# if !defined(_MSC_VER) || _MSC_VER >= 1900
#  define BOOST_CONSTEXPR constexpr
# endif
#endif
#ifndef BOOST_CONSTEXPR
# define BOOST_CONSTEXPR
#endif

#ifndef BOOST_CONSTEXPR_OR_CONST
# if !defined(_MSC_VER) || _MSC_VER >= 1900
#  define BOOST_CONSTEXPR_OR_CONST constexpr
# endif
#endif
#ifndef BOOST_CONSTEXPR_OR_CONST
# define BOOST_CONSTEXPR_OR_CONST const
#endif

namespace boost
{
  namespace spinlock
  {
    // Map in an atomic implementation
    template <class T>
    class atomic
#ifdef BOOST_SPINLOCK_USE_BOOST_ATOMIC
      : public boost::atomic<T>
    {
      typedef boost::atomic<T> Base;
#else
      : public std::atomic<T>
    {
      typedef std::atomic<T> Base;
#endif

    public:
      atomic() : Base() {}
      BOOST_CONSTEXPR atomic(T v) BOOST_NOEXCEPT : Base(std::forward<T>(v)) {}

#ifdef BOOST_NO_CXX11_DELETED_FUNCTIONS
      //private:
      atomic(const Base &) /* =delete */;
#else
      atomic(const Base &) = delete;
#endif
    };//end boost::afio::atomic
#ifdef BOOST_SPINLOCK_USE_BOOST_ATOMIC
    using boost::memory_order;
    using boost::memory_order_relaxed;
    using boost::memory_order_consume;
    using boost::memory_order_acquire;
    using boost::memory_order_release;
    using boost::memory_order_acq_rel;
    using boost::memory_order_seq_cst;
#else
    using std::memory_order;
    using std::memory_order_relaxed;
    using std::memory_order_consume;
    using std::memory_order_acquire;
    using std::memory_order_release;
    using std::memory_order_acq_rel;
    using std::memory_order_seq_cst;
#endif
    // Map in a this_thread implementation
#ifdef BOOST_SPINLOCK_USE_BOOST_THREAD
    namespace this_thread=boost::this_thread;
    namespace chrono { using boost::chrono::milliseconds; }
#else
    namespace this_thread=std::this_thread;
    namespace chrono { using std::chrono::milliseconds; }
#endif


    /*! \struct lockable_ptr
     * \brief Lets you use a pointer to memory as a spinlock :)
     */
    template<typename T> struct lockable_ptr : atomic<T *>
    {
      lockable_ptr(T *v=nullptr) : atomic<T *>(v) { }
      //! Returns the memory pointer part of the atomic
      T *get() BOOST_NOEXCEPT_OR_NOTHROW
      {
        union
        {
          T *v;
          size_t n;
        } value;
        value.v=atomic<T *>::load();
        value.n&=~(size_t)1;
        return value.v;
      }
      //! Returns the memory pointer part of the atomic
      const T *get() const BOOST_NOEXCEPT_OR_NOTHROW
      {
        union
        {
          T *v;
          size_t n;
        } value;
        value.v=atomic<T *>::load();
        value.n&=~(size_t)1;
        return value.v;
      }
      T &operator*() BOOST_NOEXCEPT_OR_NOTHROW { return *get(); }
      const T &operator*() const BOOST_NOEXCEPT_OR_NOTHROW { return *get(); }
      T *operator->() BOOST_NOEXCEPT_OR_NOTHROW { return get(); }
      const T *operator->() const BOOST_NOEXCEPT_OR_NOTHROW { return get(); }
    };
    template<typename T> struct spinlockbase
    {
    private:
      volatile atomic<T> v;
    public:
      typedef T value_type;
      spinlockbase() BOOST_NOEXCEPT_OR_NOTHROW : v(0) { }
      spinlockbase(const spinlockbase &) = delete;
      //! Atomically move constructs
      spinlockbase(spinlockbase &&o) BOOST_NOEXCEPT_OR_NOTHROW
      {
        v.store(o.v.exchange(0, memory_order_acq_rel));
      }
        //! Returns the raw atomic
      T load(memory_order o=memory_order_seq_cst) BOOST_NOEXCEPT_OR_NOTHROW { return v.load(o); }
      //! Sets the raw atomic
      void store(T a, memory_order o=memory_order_seq_cst) BOOST_NOEXCEPT_OR_NOTHROW { v.store(a, o); }
      bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW
      {
        if(v.load()) // Avoid unnecessary cache line invalidation traffic
          return false;
        T expected=0;
        return v.compare_exchange_weak(expected, 1);
      }
      void unlock() BOOST_NOEXCEPT_OR_NOTHROW
      {
        v.store(0);
      }
      bool int_yield(size_t) BOOST_NOEXCEPT_OR_NOTHROW { return false; }
    };
    template<typename T> struct spinlockbase<lockable_ptr<T>>
    {
    private:
      lockable_ptr<T> v;
    public:
      typedef T *value_type;
      spinlockbase() BOOST_NOEXCEPT_OR_NOTHROW { }
      spinlockbase(const spinlockbase &) = delete;
      //! Atomically move constructs
      spinlockbase(spinlockbase &&o) BOOST_NOEXCEPT_OR_NOTHROW
      {
        v.store(o.v.exchange(nullptr, memory_order_acq_rel));
      }
      //! Returns the memory pointer part of the atomic
      T *get() BOOST_NOEXCEPT_OR_NOTHROW { return v.get(); }
      T *operator->() BOOST_NOEXCEPT_OR_NOTHROW { return get(); }
      //! Returns the raw atomic
      T *load(memory_order o=memory_order_seq_cst) BOOST_NOEXCEPT_OR_NOTHROW { return v.load(o); }
      //! Sets the memory pointer part of the atomic preserving lockedness
      void set(T *a) BOOST_NOEXCEPT_OR_NOTHROW
      {
        union
        {
          T *v;
          size_t n;
        } value;
        T *expected;
        do
        {
          value.v=v.load();
          expected=value.v;
          bool locked=value.n&1;
          value.v=a;
          if(locked) value.n|=1;
        } while(!v.compare_exchange_weak(expected, value.v));
      }
      //! Sets the raw atomic
      void store(T *a, memory_order o=memory_order_seq_cst) BOOST_NOEXCEPT_OR_NOTHROW { v.store(a, o); }
      bool try_lock() BOOST_NOEXCEPT_OR_NOTHROW
      {
        union
        {
          T *v;
          size_t n;
        } value;
        value.v=v.load();
        if(value.n&1) // Avoid unnecessary cache line invalidation traffic
          return false;
        T *expected=value.v;
        value.n|=1;
        return v.compare_exchange_weak(expected, value.v);
      }
      void unlock() BOOST_NOEXCEPT_OR_NOTHROW
      {
        union
        {
          T *v;
          size_t n;
        } value;
        value.v=v.load();
        assert(value.n&1);
        value.n&=~(size_t)1;
        v.store(value.v);
      }
      bool int_yield(size_t) BOOST_NOEXCEPT_OR_NOTHROW { return false; }
    };
    //! \brief How many spins to try memory transactions
    template<size_t spins> struct spins_to_transact
    {
      template<class parenttype> struct policy : parenttype
      {
        static BOOST_CONSTEXPR_OR_CONST size_t spins_to_transact=spins;
        policy() {}
        policy(const policy &) = delete;
        policy(policy &&o) BOOST_NOEXCEPT : parenttype(std::move(o)) { }
      };
    };
    //! \brief How many spins to loop, optionally calling the SMT pause instruction on Intel
    template<size_t spins, bool use_pause=true> struct spins_to_loop
    {
      template<class parenttype> struct policy : parenttype
      {
        static BOOST_CONSTEXPR_OR_CONST size_t spins_to_loop=spins;
        policy() {}
        policy(const policy &) = delete;
        policy(policy &&o) BOOST_NOEXCEPT : parenttype(std::move(o)) { }
        bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
        {
          if(parenttype::int_yield(n)) return true;
          if(n>=spins) return false;
          if(use_pause)
          {
#ifdef BOOST_SMT_PAUSE
            BOOST_SMT_PAUSE;
#endif
          }
          return true;
        }
      };
    };
    //! \brief How many spins to yield the current thread's timeslice
    template<size_t spins> struct spins_to_yield
    {
      template<class parenttype> struct policy : parenttype
      {
        static BOOST_CONSTEXPR_OR_CONST size_t spins_to_yield=spins;
        policy() {}
        policy(const policy &) = delete;
        policy(policy &&o) BOOST_NOEXCEPT : parenttype(std::move(o)) { }
        bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
        {
          if(parenttype::int_yield(n)) return true;
          if(n>=spins) return false;
          this_thread::yield();
          return true;
        }
      };
    };
    //! \brief How many spins to sleep the current thread
    struct spins_to_sleep
    {
      template<class parenttype> struct policy : parenttype
      {
        policy() {}
        policy(const policy &) = delete;
        policy(policy &&o) BOOST_NOEXCEPT : parenttype(std::move(o)) { }
        bool int_yield(size_t n) BOOST_NOEXCEPT_OR_NOTHROW
        {
          if(parenttype::int_yield(n)) return true;
          this_thread::sleep_for(chrono::milliseconds(1));
          return true;
        }
      };
    };
    //! \brief A spin policy which does nothing
    struct null_spin_policy
    {
      template<class parenttype> struct policy : parenttype
      {
      };
    };
    /*! \class spinlock
    
    Meets the requirements of BasicLockable and Lockable. Also provides a get() and set() for the
    type used for the spin lock.

    So what's wrong with boost/smart_ptr/detail/spinlock.hpp then, and why
    reinvent the wheel?

    1. Non-configurable spin. AFIO needs a bigger spin than smart_ptr provides.

    2. AFIO is C++ 11, and therefore can implement this in pure C++ 11 atomics.

    3. I don't much care for doing writes during the spin. It generates an
    unnecessary amount of cache line invalidation traffic. Better to spin-read
    and only write when the read suggests you might have a chance.
    
    4. This spin lock can use a pointer to memory as the spin lock. See locked_ptr<T>.

    5. The other spin lock doesn't use Intel TSX yet.
    */
    template<typename T, template<class> class spinpolicy1=spins_to_transact<5>::policy, template<class> class spinpolicy2=spins_to_loop<125>::policy, template<class> class spinpolicy3=spins_to_yield<250>::policy, template<class> class spinpolicy4=spins_to_sleep::policy> class spinlock : public spinpolicy4<spinpolicy3<spinpolicy2<spinpolicy1<spinlockbase<T>>>>>
    {
      typedef spinpolicy4<spinpolicy3<spinpolicy2<spinpolicy1<spinlockbase<T>>>>> parenttype;
    public:
      spinlock() { }
      spinlock(const spinlock &) = delete;
      spinlock(spinlock &&o) BOOST_NOEXCEPT : parenttype(std::move(o)) { }
      void lock() BOOST_NOEXCEPT_OR_NOTHROW
      {
        for(size_t n=0;; n++)
        {
          if(parenttype::try_lock())
            return;
          parenttype::int_yield(n);
        }
      }
    };

    //! \brief Determines if a lockable is locked. Type specialise this for performance if your lockable allows examination.
    template<class T> inline bool is_lockable_locked(T &lockable) BOOST_NOEXCEPT_OR_NOTHROW
    {
      if(lockable.try_lock())
      {
        lockable.unlock();
        return true;
      }
      return false;
    }
    // For when used with a spinlock
    template<class T, template<class> class spinpolicy1, template<class> class spinpolicy2, template<class> class spinpolicy3, template<class> class spinpolicy4> inline bool is_lockable_locked(spinlock<T, spinpolicy1, spinpolicy2, spinpolicy3, spinpolicy4> &lockable) BOOST_NOEXCEPT_OR_NOTHROW
    {
      return ((size_t) lockable.load())&1;
    }

#if defined(BOOST_USING_INTEL_TSX)
    // Adapted from http://software.intel.com/en-us/articles/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family
    namespace intel_stuff
    {
      inline void run_cpuid(uint32_t eax, uint32_t ecx, uint32_t* abcd) BOOST_NOEXCEPT_OR_NOTHROW
      {
#if defined(_MSC_VER)
          __cpuidex((int *) abcd, eax, ecx);
#else
          uint32_t ebx=0, edx=0;
# if defined( __i386__ ) && defined ( __PIC__ )
          /* in case of PIC under 32-bit EBX cannot be clobbered */
          __asm__("movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi" : "=D" (ebx),
# else
          __asm__("cpuid" : "+b" (ebx),
# endif
          "+a" (eax), "+c" (ecx), "=d" (edx));
          abcd[0] = eax; abcd[1] = ebx; abcd[2] = ecx; abcd[3] = edx;
#endif
      }
      static int have_intel_tsx_support_result; // public only for benchmarking
      inline bool have_intel_tsx_support() BOOST_NOEXCEPT_OR_NOTHROW
      {
        if(have_intel_tsx_support_result) return have_intel_tsx_support_result==2;

        uint32_t abcd[4];
        uint32_t rtm_mask = (1 << 11);

        /*  CPUID.(EAX=07H, ECX=0H):EBX.RTM[bit 11]==1  */
        run_cpuid(7, 0, abcd);
        if((abcd[1] & rtm_mask) != rtm_mask)
            have_intel_tsx_support_result=1;
        else
            have_intel_tsx_support_result=2;
        return have_intel_tsx_support_result==2;
      }
    }
#endif

#ifndef BOOST_BEGIN_TRANSACT_LOCK
#ifdef BOOST_HAVE_TRANSACTIONAL_MEMORY_COMPILER
#undef BOOST_USING_INTEL_TSX
#define BOOST_BEGIN_TRANSACT_LOCK(lockable, ...) __transaction_relaxed { (void) boost::spinlock::is_lockable_locked(lockable);
#define BOOST_END_TRANSACT_LOCK(lockable) }
#define BOOST_BEGIN_NESTED_TRANSACT_LOCK(N) __transaction_relaxed
#define BOOST_END_NESTED_TRANSACT_LOCK(N)
#elif defined(BOOST_USING_INTEL_TSX)

#define BOOST_MEMORY_TRANSACTIONS_XBEGIN_STARTED   (~0u)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_EXPLICIT  (1 << 0)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_RETRY     (1 << 1)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_CONFLICT  (1 << 2)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_CAPACITY  (1 << 3)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_DEBUG     (1 << 4)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_NESTED    (1 << 5)
#define BOOST_MEMORY_TRANSACTIONS_XABORT_CODE(x)   ((unsigned char)(((x) >> 24) & 0xFF))
            
#if defined(__has_feature)
# if __has_feature(thread_sanitizer) // If this is being thread sanitised, never use memory transactions
#  define BOOST_MEMORY_TRANSACTIONS_DISABLE_INTEL_TSX
# endif
#endif

#if defined(_MSC_VER)
    // Declare MSVC intrinsics
    extern "C" unsigned int  _xbegin(void);
    extern "C" void          _xend(void);
    extern "C" void          _xabort(const unsigned int);
    extern "C" unsigned char _xtest(void);
#define BOOST_MEMORY_TRANSACTIONS_XBEGIN(...) _xbegin(__VA_ARGS__)
#define BOOST_MEMORY_TRANSACTIONS_XEND(...) _xend(__VA_ARGS__)
#define BOOST_MEMORY_TRANSACTIONS_XABORT(...) _xabort(__VA_ARGS__)
#define BOOST_MEMORY_TRANSACTIONS_XTEST(...) _xtest(__VA_ARGS__)

#elif defined(__GNUC__) && ( defined(__i386__) || defined(__x86_64__) )

    // Hack the bytes codes in for older compilers to avoid needing to compile with -mrtm
    namespace { // prevent any collisions with <immintrin.h>
      static __attribute__((__always_inline__)) inline unsigned int _xbegin() BOOST_NOEXCEPT_OR_NOTHROW
      {
        unsigned int ret = BOOST_MEMORY_TRANSACTIONS_XBEGIN_STARTED;
        asm volatile(".byte 0xc7,0xf8 ; .long 0" : "+a" (ret) :: "memory");
        return ret;
      }

      static __attribute__((__always_inline__)) inline unsigned char _xtest() BOOST_NOEXCEPT_OR_NOTHROW
      {
        unsigned char out;
        asm volatile(".byte 0x0f,0x01,0xd6 ; setnz %0" : "=r" (out) :: "memory");
        return out;
      }
    }
#define BOOST_MEMORY_TRANSACTIONS_XBEGIN(...) _xbegin(__VA_ARGS__)
#define BOOST_MEMORY_TRANSACTIONS_XEND(...) asm volatile(".byte 0x0f,0x01,0xd5" ::: "memory")
#define BOOST_MEMORY_TRANSACTIONS_XABORT(status) asm volatile(".byte 0xc6,0xf8,%P0" :: "i" (status) : "memory")
#define BOOST_MEMORY_TRANSACTIONS_XTEST(...) _xtest(__VA_ARGS__)
#endif

    template<class T> struct get_spins_to_transact
    {
      static BOOST_CONSTEXPR_OR_CONST size_t value=4;
    };
    template<class T, template<class> class spinpolicy1, template<class> class spinpolicy2, template<class> class spinpolicy3, template<class> class spinpolicy4> struct get_spins_to_transact<spinlock<T, spinpolicy1, spinpolicy2, spinpolicy3, spinpolicy4>>
    {
      static BOOST_CONSTEXPR_OR_CONST size_t value=spinlock<T, spinpolicy1, spinpolicy2, spinpolicy3, spinpolicy4>::spins_to_transact;
    };
    template<class T> struct intel_tsx_transaction_impl
    {
    protected:
      T &lockable;
      int dismissed; // 0=disabled, 1=abort on exception throw, 2=commit, 3=traditional locks
      intel_tsx_transaction_impl(T &_lockable) : lockable(_lockable), dismissed(0) { }
    public:
      intel_tsx_transaction_impl(intel_tsx_transaction_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW : lockable(o.lockable), dismissed(o.dismissed)
      {
        o.dismissed=0; // disable original
      }
      void commit() BOOST_NOEXCEPT_OR_NOTHROW
      {
        if(1==dismissed)
          dismissed=2; // commit transaction
      }
    };
    template<class T> struct intel_tsx_transaction : public intel_tsx_transaction_impl<T>
    {
      typedef intel_tsx_transaction_impl<T> Base;
      template<class Pred> explicit intel_tsx_transaction(T &_lockable, Pred &&pred) : Base(_lockable)
      {
        // Try only a certain number of times
        size_t spins_to_transact=pred() ? get_spins_to_transact<T>::value : 0;
        for(size_t n=0; n<spins_to_transact; n++)
        {
          unsigned state=BOOST_MEMORY_TRANSACTIONS_XABORT_CAPACITY;
#ifndef BOOST_MEMORY_TRANSACTIONS_DISABLE_INTEL_TSX
          if(intel_stuff::have_intel_tsx_support())
            state=BOOST_MEMORY_TRANSACTIONS_XBEGIN(); // start transaction, or cope with abort
#endif
          if(BOOST_MEMORY_TRANSACTIONS_XBEGIN_STARTED==state)
          {
            if(!is_lockable_locked(Base::lockable))
            {
              // Lock is free, so we can proceed with the transaction
              Base::dismissed=1; // set to abort on exception throw
              return;
            }
            // If lock is not free, we need to abort transaction as something else is running
#if 1
            BOOST_MEMORY_TRANSACTIONS_XABORT(0x79);
#else
            BOOST_MEMORY_TRANSACTIONS_XEND();
#endif
            continue;
            // Never reaches this point
          }
          //std::cerr << "A=" << std::hex << state << std::endl;
          // Transaction aborted due to too many locks or hard abort?
          if((state & BOOST_MEMORY_TRANSACTIONS_XABORT_CAPACITY) || !(state & BOOST_MEMORY_TRANSACTIONS_XABORT_RETRY))
          {
            // Fall back onto pure locks
            break;
          }
          // Was it me who aborted?
          if((state & BOOST_MEMORY_TRANSACTIONS_XABORT_EXPLICIT) && !(state & BOOST_MEMORY_TRANSACTIONS_XABORT_NESTED))
          {
            switch(BOOST_MEMORY_TRANSACTIONS_XABORT_CODE(state))
            {
            case 0x78: // exception thrown
              throw std::runtime_error("Unknown exception thrown inside Intel TSX memory transaction");
            case 0x79: // my lock was held by someone else, so repeat
              break;
            default: // something else aborted. Best fall back to locks
              n=((size_t) 1<<(4*sizeof(n)))-1;
              break;
            }
          }
        }
        // If the loop exited, we're falling back onto traditional locks
        Base::lockable.lock();
        Base::dismissed=3;
      }
      ~intel_tsx_transaction() BOOST_NOEXCEPT_OR_NOTHROW
      {
        if(Base::dismissed)
        {
          if(1==Base::dismissed)
            BOOST_MEMORY_TRANSACTIONS_XABORT(0x78);
          else if(2==Base::dismissed)
          {
            BOOST_MEMORY_TRANSACTIONS_XEND();
            //std::cerr << "TC" << std::endl;
          }
          else if(3==Base::dismissed)
            Base::lockable.unlock();
        }
      }
      intel_tsx_transaction(intel_tsx_transaction &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::move(o)) { }
  private:
      intel_tsx_transaction(const intel_tsx_transaction &)
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
      = delete
#endif
      ;
    };
    // For nested transactions
    template<class T> struct intel_tsx_transaction<intel_tsx_transaction<T>> : public intel_tsx_transaction_impl<intel_tsx_transaction<T>>
    {
      typedef intel_tsx_transaction_impl<intel_tsx_transaction<T>> Base;
      explicit intel_tsx_transaction(intel_tsx_transaction<T> &_lockable) : Base(_lockable)
      {
        // Try only a certain number of times
        size_t spins_to_transact=get_spins_to_transact<T>::value;
        for(size_t n=0; n<spins_to_transact; n++)
        {
          unsigned state=BOOST_MEMORY_TRANSACTIONS_XABORT_CAPACITY;
          if(intel_stuff::have_intel_tsx_support())
            state=BOOST_MEMORY_TRANSACTIONS_XBEGIN(); // start transaction, or cope with abort
          if(BOOST_MEMORY_TRANSACTIONS_XBEGIN_STARTED==state)
          {
            Base::dismissed=1; // set to abort on exception throw
            return;
          }
          //std::cerr << "A=" << std::hex << state << std::endl;
          // Transaction aborted due to too many locks or hard abort?
          if((state & BOOST_MEMORY_TRANSACTIONS_XABORT_CAPACITY) || !(state & BOOST_MEMORY_TRANSACTIONS_XABORT_RETRY))
          {
            // Fall back onto pure locks
            break;
          }
          // Was it me who aborted?
          if((state & BOOST_MEMORY_TRANSACTIONS_XABORT_EXPLICIT) && !(state & BOOST_MEMORY_TRANSACTIONS_XABORT_NESTED))
          {
            switch(BOOST_MEMORY_TRANSACTIONS_XABORT_CODE(state))
            {
            case 0x78: // exception thrown
              throw std::runtime_error("Unknown exception thrown inside Intel TSX memory transaction");
            case 0x79: // my lock was held by someone else, so repeat
              break;
            default: // something else aborted. Best fall back to locks
              n=((size_t) 1<<(4*sizeof(n)))-1;
              break;
            }
          }
        }
        Base::dismissed=3;
      }
      ~intel_tsx_transaction() BOOST_NOEXCEPT_OR_NOTHROW
      {
        if(Base::dismissed)
        {
          if(1==Base::dismissed)
            BOOST_MEMORY_TRANSACTIONS_XABORT(0x78);
          else if(2==Base::dismissed)
          {
            BOOST_MEMORY_TRANSACTIONS_XEND();
            //std::cerr << "TC" << std::endl;
          }
        }
      }
      intel_tsx_transaction(intel_tsx_transaction &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::move(o)) { }
  private:
      intel_tsx_transaction(const intel_tsx_transaction &)
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
      = delete
#endif
      ;
    };
    template<class T> inline intel_tsx_transaction<T> make_intel_tsx_transaction(T &lockable) BOOST_NOEXCEPT_OR_NOTHROW
    {
      return intel_tsx_transaction<T>(lockable, []{ return true; });
    }
    template<class T, class Pred> inline intel_tsx_transaction<T> make_intel_tsx_transaction(T &lockable, Pred &&pred) BOOST_NOEXCEPT_OR_NOTHROW
    {
      return intel_tsx_transaction<T>(lockable, pred);
    }
#define BOOST_BEGIN_TRANSACT_LOCK(...) { auto __tsx_transaction(boost::spinlock::make_intel_tsx_transaction(__VA_ARGS__)); {
#define BOOST_END_TRANSACT_LOCK(lockable) } __tsx_transaction.commit(); }
#define BOOST_BEGIN_NESTED_TRANSACT_LOCK(N) { auto __tsx_transaction##N(boost::spinlock::make_intel_tsx_transaction(__tsx_transaction)); {
#define BOOST_END_NESTED_TRANSACT_LOCK(N) } __tsx_transaction##N.commit(); }

#endif // BOOST_USING_INTEL_TSX
#endif // BOOST_BEGIN_TRANSACT_LOCK

#ifndef BOOST_BEGIN_TRANSACT_LOCK
#define BOOST_BEGIN_TRANSACT_LOCK(lockable ...) { boost::lock_guard<decltype(lockable)> __tsx_transaction(lockable);
#define BOOST_END_TRANSACT_LOCK(lockable) }
#define BOOST_BEGIN_NESTED_TRANSACT_LOCK(N)
#define BOOST_END_NESTED_TRANSACT_LOCK(N)
#endif // BOOST_BEGIN_TRANSACT_LOCK

  }
}

#endif // BOOST_SPINLOCK_HPP
