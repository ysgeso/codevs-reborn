#pragma once
#pragma once

// simd を利用するためのコード

#include <immintrin.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#endif

using namespace std;

// avx が使える場合に有効にすること
#define USE_AVX

// visual studio で開発する場合
#if defined(_MSC_VER)
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#endif

#ifdef USE_AVX
const int alignsize = 32;

#define SETZERO256()	_mm256_setzero_si256()
#define AND256(x, y)	_mm256_and_si256(x, y)
#define OR256(x, y)		_mm256_or_si256(x, y)
#define XOR256(x, y)	_mm256_xor_si256(x, y)
#define ANDNOT256(x, y)	_mm256_andnot_si256(x, y)
#define TESTC256(x, y)	_mm256_testc_si256(x, y)
// 等しいかどうかのチェック
// なお、これはすべてが 1 のビットのM256を作成するために使う。等しいかどうかの比較はTEST256を使っている
#define CMPEQ256(x, y)  _mm256_cmpeq_epi64(x, y)
#else
const int alignsize = 16;
struct M256 {
	union {
		struct { uint64_t m[4]; };
	};
};
inline M256 AND256(M256 x, M256 y) { M256 m; m.m[0] = x.m[0] & y.m[0]; m.m[1] = x.m[1] & y.m[1]; m.m[2] = x.m[2] & y.m[2]; m.m[3] = x.m[3] & y.m[3]; return m; }
inline M256 OR256(M256 x, M256 y) { M256 m; m.m[0] = x.m[0] | y.m[0]; m.m[1] = x.m[1] | y.m[1]; m.m[2] = x.m[2] | y.m[2]; m.m[3] = x.m[3] | y.m[3]; return m; }
inline M256 XOR256(M256 x, M256 y) { M256 m; m.m[0] = x.m[0] ^ y.m[0]; m.m[1] = x.m[1] ^ y.m[1]; m.m[2] = x.m[2] ^ y.m[2]; m.m[3] = x.m[3] ^ y.m[3]; return m; }
// expression template でいずれ置き換える予定・・・
inline M256 ANDNOT256(M256 x, M256 y) { M256 m; m.m[0] = (~m.m[0]) & y.m[0]; m.m[1] = (~x.m[1]) & y.m[1]; m.m[2] = (~x.m[2]) & y.m[2]; m.m[3] = (~x.m[3]) & y.m[3]; return m; }
inline bool TESTC256(M256 x, M256 y) { return ((x.m[0] == y.m[0]) && (x.m[1] == y.m[1]) && (x.m[2] == y.m[2]) && (x.m[3] == y.m[3])) ? true : false; }
inline M256 NOT256(M256 x) { M256 m; m.m[0] = ~m.m[0]; m.m[1] = ~x.m[1]; m.m[2] = ~x.m[2]; m.m[3] = ~x.m[3]; return m; }
//inline M256 CMPEQ256(M256 x, M256 y) { M256 m; m.m1 = _mm_cmp_ps(x.m1, y.m1, _CMP_EQ_UQ); m.m2 = _mm_cmp_ps(x.m2, y.m2, _CMP_EQ_UQ); return m; }
// _m_cmpeq_ps は使っちゃダメ。浮動小数点数として不正なビット列があるとうまくいかない
//inline M256 CMPEQ256(M256 x, M256 y) { M256 m; m.m1i = _mm_cmpeq_epi64(x.m1i, y.m1i); m.m2i = _mm_cmpeq_epi64(x.m2i, y.m2i); return m; }
//inline M256 FULL256(M256 m1) { M256 m;  m.m1 = _mm_cmpeq_ps(m1.m1, m1.m1); m.m2 = m.m1; return m; }
#endif

// 自作のアロケータ。確保したメモリのアライメントが 32 に収まるようにしたもの。
template <class T>
struct MyAllocator {
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	// 要素の型
	using value_type = T;
	static const int alignment = alignsize;

	// 特殊関数
	// (デフォルトコンストラクタ、コピーコンストラクタ
	//  、ムーブコンストラクタ)
	MyAllocator() {}

	// 別な要素型のアロケータを受け取るコンストラクタ
	template <class U>
	MyAllocator(const MyAllocator<U>&) {}

	template <class U>
	struct rebind
	{
		typedef MyAllocator<U> other;
	};

	// 初期化済みの領域を削除する
	void destroy(pointer p)
	{
		p->~T();
	}

	// 割当て済みの領域を初期化する
	void construct(pointer p, const T& value)
	{
		new((void*)p) T(value);
	}

	// メモリ確保
	T* allocate(std::size_t n)
	{
#if defined(_MSC_VER)
		return reinterpret_cast<T*>(_aligned_malloc(sizeof(T) * n, alignment));
#else
		void* p;
		return reinterpret_cast<T *>(posix_memalign(&p, alignment, sizeof(T) * n) == 0 ? p : nullptr);
#endif
	}

	// メモリ解放
	void deallocate(T* p, std::size_t n)
	{
		static_cast<void>(n);
#if defined(_MSC_VER)
		_aligned_free(p);
#else
		std::free(p);
#endif
	}
};

#define popcnt(val) _mm_popcnt_u32(val)
#define popcnt64(val) _mm_popcnt_u64(val)
