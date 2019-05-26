#pragma once
#include "simd.hpp"
#include <limits>
#include <cstring>
// 置換表に関するコード

// これを設定しておくと、置換表に関する様々なカウントデータを記録するようになる
#define TT_COUNT

// 置換表のキーの型。64ビットとする
using Key = uint64_t;

// 置換表に記録するデータ（2のn乗のビット数にする）
//struct TT_DATA {
//	// キー（63ビット（上位1ビットを除く））
//	// 上位1ビットが立っているとき、その63ビットのキーはすでに探索済であることを表す
//	Key			key;
//
//	bool isset() const {
//		return key & 0x8000000000000000;
//	}
//
//	bool operator==(const Key& k) const
//	{
//		return (key & 0x7fffffffffffffff) == (k & 0x7fffffffffffffff);
//	};
//
//	void set(const Key& k) {
//		key = k | 0x8000000000000000;
//	}
//};

//struct TT_DATA {
//	// キー（56ビット（上位8ビットを除く））
//	// 上位8ビットが0でない時、残りの56ビットのキーはすでに探索済であることを表す
//	Key			key;
//
//	bool isset() const {
//		return key & static_cast<uint64_t>(0x00000000000000ff);
//	}
//
//	bool operator==(const Key& k) const
//	{
//		return (key & 0xffffffffffffff00) == (k & 0xffffffffffffff00);
//	};
//
//	void set(const Key& k, const uint8_t value) {
//		key = (k & 0xffffffffffffff00) | static_cast<uint64_t>(value); // 0x00000000000000ff;
//	}
//	uint8_t get() const {
//		return static_cast<uint8_t>(key & static_cast<uint64_t>(0xff));
//	}
//};

struct TT_DATA {
	// キー（56ビット（上位8ビットを除く））
	// 上位8ビットが0でない時、残りの56ビットのキーはすでに探索済であることを表す
	Key			key;
	double		data;

	bool isset() const {
		return data != 0;
	}

	bool operator==(const Key& k) const
	{
		return key == k;
	};

	void set(const Key& k, const double value) {
		key = k;
		data = value;
	}
	double_t get() const {
		return data;
	}
};


// 置換表のハッシュのビット数-1
// キャッシュの関係上、15あたりが最速っぽい？
constexpr int TT_HASH_BIT = 16;
// クラスターの数
constexpr int TT_CLUSTERCOUNT = 1 << TT_HASH_BIT;
// クラスターのサイズ
constexpr int TT_CLUSTERSIZE = 4;
struct TT_CLUSTER {
	TT_DATA data[TT_CLUSTERSIZE];
};

struct alignas(32) TT_TABLE {
	TT_CLUSTER table[TT_CLUSTERCOUNT];
#ifdef TT_COUNT
	// この置換表に関する様々なカウントを行うための変数
	int hitcount, nothitcount, conflictcount, dropcount;
	int totalhitcount, totalnothitcount, totalconflictcount, totaldropcount;
#endif
	// key に一致するキャッシュがあればそれを返す
	TT_DATA *findcache(const Key key, bool& found) {
		TT_CLUSTER *c = &table[key & (TT_CLUSTERCOUNT - 1)];
		for (int i = 0; i < TT_CLUSTERSIZE; i++) {
			// 登録済かどうかを判定する
			if (!c->data[i].isset()) {
				found = false;
#ifdef TT_COUNT
				nothitcount++;
				totalnothitcount++;
#endif
				return &c->data[i];
			}
			// 登録済の場合、keyの判定を行う
			if (c->data[i] == key) {
				found = true;
#ifdef TT_COUNT
				hitcount++;
				totalhitcount++;
				if (i > 0) {
					conflictcount++;
					totalconflictcount++;
				}
#endif
				return &c->data[i];
			}
		}
		found = false;
		for (int i = TT_CLUSTERSIZE - 1; i > 0; i--) {
			c->data[i] = c->data[i - 1];
		}
#ifdef TT_COUNT
		dropcount++;
		totaldropcount++;
		nothitcount++;
		totalnothitcount++;
		conflictcount++;
		totalconflictcount++;
#endif
		return &c->data[0];
	}
#ifdef TT_COUNT
	TT_TABLE() : totalhitcount(0), totalnothitcount(0), totalconflictcount(0), totaldropcount(0) {
#else
	TT_TABLE() {
#endif
		clear();
	}
	void clear() {
#ifdef TT_COUNT
		hitcount = 0;
		nothitcount = 0;
		dropcount = 0;
		conflictcount = 0;
#endif
		memset(&table[0], 0, sizeof(TT_CLUSTER) * TT_CLUSTERCOUNT);
	}
#ifdef TT_COUNT
	void dump_count() const { 
		cerr << "hit " << hitcount << " nothit " << nothitcount << " confilct " << conflictcount << " drop " << dropcount << endl;
	}
	void dump_totalcount() const {
		cerr << "total hit " << totalhitcount << " nothit " << totalnothitcount << " confilct " << totalconflictcount << " drop " << totaldropcount << endl;
	}
#else
	void dump_count() const {}
	void dump_totalcount() const {}
#endif
	};

// zorbisthash 値を生成する関数
Key create_hash(mt19937& mt, uniform_int_distribution<int>& rnd16) {
	Key k = 0;
	for (int i = 0; i < 4; i++) {
		k += static_cast<Key>(rnd16(mt));
		if (i != 3) {
			k <<= 16;
		}
	}
	return k;
}