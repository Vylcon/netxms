/*
** NetXMS - Network Management System
** Copyright (C) 2003-2017 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published
** by the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: nxatomic.h
**
**/

#ifndef _nxatomic_h_
#define _nxatomic_h_

#include <nms_common.h>

#ifdef __cplusplus

#ifdef __sun
#include <sys/atomic.h>
#endif

#if defined(__HP_aCC) && HAVE_ATOMIC_H
#include <atomic.h>
#endif

#ifdef _WIN32

typedef volatile LONG VolatileCounter;
typedef volatile LONGLONG VolatileCounter64;

// 64 bit atomics not available on Windows XP, implement them using _InterlockedCompareExchange64
#if !defined(_WIN64) && (_WIN32_WINNT < 0x0502)

#pragma intrinsic(_InterlockedCompareExchange64)

FORCEINLINE LONGLONG InterlockedIncrement64(LONGLONG volatile *v)
{
   LONGLONG old;
   do 
   {
      old = *v;
   } while(_InterlockedCompareExchange64(v, old + 1, old) != old);
   return old + 1;
}

FORCEINLINE LONGLONG InterlockedDecrement64(LONGLONG volatile *v)
{
   LONGLONG old;
   do 
   {
      old = *v;
   } while(_InterlockedCompareExchange64(v, old - 1, old) != old);
   return old - 1;
}

#endif

#else

#if defined(__sun)

typedef volatile uint32_t VolatileCounter;
typedef volatile uint64_t VolatileCounter64;

#if !HAVE_ATOMIC_INC_32_NV
extern "C" volatile uint32_t solaris9_atomic_inc32(volatile uint32_t *v);
#endif

#if !HAVE_ATOMIC_DEC_32_NV
extern "C" volatile uint32_t solaris9_atomic_dec32(volatile uint32_t *v);
#endif

#if !HAVE_ATOMIC_INC_64_NV
extern "C" volatile uint64_t solaris9_atomic_inc64(volatile uint64_t *v);
#endif

#if !HAVE_ATOMIC_DEC_64_NV
extern "C" volatile uint64_t solaris9_atomic_dec64(volatile uint64_t *v);
#endif

#if !HAVE_ATOMIC_SWAP_PTR
extern "C" void *solaris9_atomic_swap_ptr(volatile *void *target, void *value);
#endif

/**
 * Atomically increment 32-bit value by 1
 */
inline VolatileCounter InterlockedIncrement(VolatileCounter *v)
{
#if HAVE_ATOMIC_INC_32_NV
   return atomic_inc_32_nv(v);
#else
   return solaris9_atomic_inc32(v);
#endif
}

/**
 * Atomically decrement 32-bit value by 1
 */
inline VolatileCounter InterlockedDecrement(VolatileCounter *v)
{
#if HAVE_ATOMIC_DEC_32_NV
   return atomic_dec_32_nv(v);
#else
   return solaris9_atomic_dec32(v);
#endif
}

/**
 * Atomically increment 64-bit value by 1
 */
inline VolatileCounter64 InterlockedIncrement64(VolatileCounter64 *v)
{
#if HAVE_ATOMIC_INC_64_NV
   return atomic_inc_64_nv(v);
#else
   return solaris9_atomic_inc64(v);
#endif
}

/**
 * Atomically decrement 64-bit value by 1
 */
inline VolatileCounter64 InterlockedDecrement64(VolatileCounter64 *v)
{
#if HAVE_ATOMIC_DEC_64_NV
   return atomic_dec_64_nv(v);
#else
   return solaris9_atomic_dec64(v);
#endif
}

/**
 * Atomically set pointer
 */
inline void *InterlockedExchangePointer(void *volatile *target, void *value)
{
#if HAVE_ATOMIC_SWAP_PTR
   return atomic_swap_ptr(target, value);
#else
   return solaris9_atomic_swap_ptr(target, value);
#endif
}

#elif defined(__HP_aCC)

typedef volatile uint32_t VolatileCounter;
typedef volatile uint64_t VolatileCounter64;

#if defined(__hppa) && !HAVE_ATOMIC_H
VolatileCounter parisc_atomic_inc(VolatileCounter *v);
VolatileCounter parisc_atomic_dec(VolatileCounter *v);
VolatileCounter64 parisc_atomic_inc64(VolatileCounter64 *v);
VolatileCounter64 parisc_atomic_dec64(VolatileCounter64 *v);
void *parisc_atomic_swap_ptr(void *volatile *p, void *v);
#endif

/**
 * Atomically increment 32-bit value by 1
 */
inline VolatileCounter InterlockedIncrement(VolatileCounter *v)
{
#if HAVE_ATOMIC_H
   return atomic_inc_32(v) + 1;
#elif defined(__hppa)
   return parisc_atomic_inc(v);
#else
   _Asm_mf(_DFLT_FENCE);
   return (uint32_t)_Asm_fetchadd(_FASZ_W, _SEM_ACQ, (void *)v, +1, _LDHINT_NONE) + 1;
#endif
}

/**
 * Atomically decrement 32-bit value by 1
 */
inline VolatileCounter InterlockedDecrement(VolatileCounter *v)
{
#if HAVE_ATOMIC_H
   return atomic_dec_32(v) - 1;
#elif defined(__hppa)
   return parisc_atomic_dec(v);
#else
   _Asm_mf(_DFLT_FENCE);
   return (uint32_t)_Asm_fetchadd(_FASZ_W, _SEM_ACQ, (void *)v, -1, _LDHINT_NONE) - 1;
#endif
}

/**
 * Atomically increment 64-bit value by 1
 */
inline VolatileCounter64 InterlockedIncrement64(VolatileCounter64 *v)
{
#if HAVE_ATOMIC_H
   return atomic_inc_64(v) + 1;
#elif defined(__hppa)
   return parisc_atomic_inc64(v);
#else
   _Asm_mf(_DFLT_FENCE);
   return (uint64_t)_Asm_fetchadd(_FASZ_D, _SEM_ACQ, (void *)v, +1, _LDHINT_NONE) + 1;
#endif
}

/**
 * Atomically decrement 64-bit value by 1
 */
inline VolatileCounter64 InterlockedDecrement64(VolatileCounter64 *v)
{
#if HAVE_ATOMIC_H
   return atomic_dec_64(v) - 1;
#elif defined(__hppa)
   return parisc_atomic_dec64(v);
#else
   _Asm_mf(_DFLT_FENCE);
   return (uint64_t)_Asm_fetchadd(_FASZ_D, _SEM_ACQ, (void *)v, -1, _LDHINT_NONE) - 1;
#endif
}

/**
 * Atomically set pointer
 */
inline void *InterlockedExchangePointer(void *volatile *target, void *value)
{
#ifdef __64BIT__
#if HAVE_ATOMIC_H
   return (void*)atomic_swap_64((uint64_t*)target, (uint64_t)value);
#elif defined(__hppa)
   return parisc_atomic_swap_ptr(target, value);
#else
   _Asm_mf(_DFLT_FENCE);
   return (void*)_Asm_xchg(_SZ_D, (void*)target, (uint64_t)value, _LDHINT_NONE);
#endif
#else /* __64BIT__ */
#if HAVE_ATOMIC_H
   return (void*)atomic_swap_32((uint32_t*)target, (uint32_t)value);
#elif defined(__hppa)
   return parisc_atomic_swap_ptr(target, value);
#else
   _Asm_mf(_DFLT_FENCE);
   return (void*)_Asm_xchg(_SZ_W, (void*)target, (uint32_t)value, _LDHINT_NONE);
#endif
#endif
}

#elif defined(__IBMC__) || defined(__IBMCPP__)

typedef volatile INT32 VolatileCounter;
typedef volatile INT64 VolatileCounter64;

/**
 * Atomically increment 32-bit value by 1
 */
inline VolatileCounter InterlockedIncrement(VolatileCounter *v)
{
#if !HAVE_DECL___SYNC_ADD_AND_FETCH
   VolatileCounter oldval;
   do
   {
      oldval = __lwarx(v);
   } while(__stwcx(v, oldval + 1) == 0);
   return oldval + 1;
#else
   return __sync_add_and_fetch(v, 1);
#endif
}

/**
 * Atomically decrement 32-bit value by 1
 */
inline VolatileCounter InterlockedDecrement(VolatileCounter *v)
{
#if !HAVE_DECL___SYNC_SUB_AND_FETCH
   VolatileCounter oldval;
   do
   {
      oldval = __lwarx(v);
   } while(__stwcx(v, oldval - 1) == 0);
   return oldval - 1;
#else
   return __sync_sub_and_fetch(v, 1);
#endif
}

/**
 * Atomically increment 64-bit value by 1
 */
inline VolatileCounter64 InterlockedIncrement64(VolatileCounter64 *v)
{
#if !HAVE_DECL___SYNC_ADD_AND_FETCH
   VolatileCounter64 oldval;
   do
   {
      oldval = __ldarx(v);
   } while(__stdcx(v, oldval + 1) == 0);
   return oldval + 1;
#else
   return __sync_add_and_fetch(v, 1);
#endif
}

/**
 * Atomically decrement 64-bit value by 1
 */
inline VolatileCounter64 InterlockedDecrement64(VolatileCounter64 *v)
{
#if !HAVE_DECL___SYNC_SUB_AND_FETCH
   VolatileCounter64 oldval;
   do
   {
      oldval = __ldarx(v);
   } while(__stdcx(v, oldval - 1) == 0);
   return oldval - 1;
#else
   return __sync_sub_and_fetch(v, 1);
#endif
}

/**
 * Atomically set pointer
 */
inline void *InterlockedExchangePointer(void *volatile *target, void *value)
{
   void *oldval;
   do
   {
#ifdef __64BIT__
      oldval = (void *)__ldarx((long *)target);
#else
      oldval = (void *)__lwarx((int *)target);
#endif
#ifdef __64BIT__
   } while(__stdcx((long *)target, (long)value) == 0);
#else
   } while(__stwcx((int *)target, (int)value) == 0);
#endif
   return oldval;
}

#else /* not Solaris nor HP-UX nor AIX */

typedef volatile INT32 VolatileCounter;
typedef volatile INT64 VolatileCounter64;

/**
 * Atomically increment 32-bit value by 1
 */
inline VolatileCounter InterlockedIncrement(VolatileCounter *v)
{
#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC_MINOR__ < 1)) && (defined(__i386__) || defined(__x86_64__))
   VolatileCounter temp = 1;
   __asm__ __volatile__("lock; xaddl %0,%1" : "+r" (temp), "+m" (*v) : : "memory");
   return temp + 1;
#else
   return __sync_add_and_fetch(v, 1);
#endif
}

/**
 * Atomically decrement 32-bit value by 1
 */
inline VolatileCounter InterlockedDecrement(VolatileCounter *v)
{
#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC_MINOR__ < 1)) && (defined(__i386__) || defined(__x86_64__))
   VolatileCounter temp = -1;
   __asm__ __volatile__("lock; xaddl %0,%1" : "+r" (temp), "+m" (*v) : : "memory");
   return temp - 1;
#else
   return __sync_sub_and_fetch(v, 1);
#endif
}

/**
 * Atomically increment 64-bit value by 1
 */
inline VolatileCounter64 InterlockedIncrement64(VolatileCounter64 *v)
{
#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC_MINOR__ < 1)) && (defined(__i386__) || defined(__x86_64__))
   VolatileCounter64 temp = 1;
   __asm__ __volatile__("lock; xaddq %0,%1" : "+r" (temp), "+m" (*v) : : "memory");
   return temp + 1;
#else
   return __sync_add_and_fetch(v, 1);
#endif
}

/**
 * Atomically decrement 64-bit value by 1
 */
inline VolatileCounter64 InterlockedDecrement64(VolatileCounter64 *v)
{
#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC_MINOR__ < 1)) && (defined(__i386__) || defined(__x86_64__))
   VolatileCounter64 temp = -1;
   __asm__ __volatile__("lock; xaddq %0,%1" : "+r" (temp), "+m" (*v) : : "memory");
   return temp - 1;
#else
   return __sync_sub_and_fetch(v, 1);
#endif
}

/**
 * Atomically set pointer
 */
inline void *InterlockedExchangePointer(void* volatile *target, void *value)
{
#if defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC_MINOR__ < 1)) && (defined(__i386__) || defined(__x86_64__))
   void *oldval;
#ifdef __64BIT__
   __asm__ __volatile__("xchgq %q2, %1" : "=a" (oldval), "+m" (*target) : "0" (value));
#else
   __asm__ __volatile__("xchgl %2, %1" : "=a" (oldval), "+m" (*target) : "0" (value));
#endif
   return oldval;
#else
   __sync_synchronize();
   return __sync_lock_test_and_set(target, value);
#endif
}

#endif   /* __sun */

#endif   /* _WIN32 */

/**
 * Atomically set pointer - helper template
 */
template<typename T> T *InterlockedExchangeObjectPointer(T* volatile *target, T *value)
{
   return static_cast<T*>(InterlockedExchangePointer(reinterpret_cast<void* volatile *>(target), value));
}

#endif   /* __cplusplus */

#endif
