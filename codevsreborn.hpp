#pragma once
#include <iostream>
#include <utility>
#include <list>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cassert>
#include <string>
#include <random>
#include <cstring>
#include <algorithm>
#include <thread>
#include <mutex>
#include <map>
#include "simd.hpp"

// ゲーム盤の幅
constexpr int BOARD_WIDTH = 10;
// ゲーム盤の高さ（お邪魔ブロックが落ちてきたあと、パックを落とす場合があるので、16+3にする必要がある）
constexpr int BOARD_HEIGHT = 19;
// 毎ターン送られてくるゲーム情報のゲーム盤の高さ
constexpr int LOAD_BOARD_HEIGHT = 16;
// ゲームオーバーとなる高さ
constexpr int GAMEOVER_Y = 17;
// 最大ターン数
constexpr int MAX_TURN = 500;

// 全探索時の探索最大深さ（実行時間的には5が限界の模様）
constexpr int MAX_DEPTH = 6;
// ビームサーチ時の探索最大深さ（かなり多めにとっておく）
constexpr int MAX_BEAM_DEPTH = 70;

// double の無限大
constexpr double INF = std::numeric_limits<double>::infinity();

// デバッグ関連の処理を行うかどうか
//#define DEBUG

// 時間を計測するクラス
struct Timer {
	std::chrono::system_clock::time_point  start;
public:
	Timer() : start(std::chrono::system_clock::now()) {}

	// 時間の初期化
	void restart() {
		start = std::chrono::system_clock::now();
	}

	// 経過時間（ミリ秒）を返す
	int time() const {
		auto now = std::chrono::system_clock::now();  // 計測終了時間
		return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count()); //処理に要した時間をミリ秒に変換
	}
};

// 盤面の一部の状態を表すビットボード
// 盤面の座標は左下を(0, 0)とし、右がx座標の正の方向、上がy座標の正の方向とする（ルールの記述とと上下が逆。pext_u32を使いたいのでそうした）
// ビットボードは、各列24ビット分を、x座標の小さい順に、ビットボードの下位ビットから順に積んでいく
// 盤の幅は10、高さは最大16+3=19ビット分のデータが必要
// 1列分のデータとして、24ビット確保することで、24*10=240で、256ビット内に収めることが可能
// このコードは、IntelのCPUなど、リトルエンディアンの場合に特化してあるので、ビッグエンディアンのCPUの場合はうまく動かないはず
// 開発PC及び、サブミット対戦環境のCPUはリトルエンディアンのようなので、リトルエンディアンのみ対応する
// （補足：同じものを512ビットで作った場合と比べて 約20% くらい速いようだ）
struct alignas(alignsize)BitBoard {
	// コンストラクタ。特に何もしない
	BitBoard() {}
	// 値を設定するコンストラクタ
	BitBoard(__m256i bb ) : data(bb) {}

	// 中身を 0 クリアする
	void clear() {
		data = SETZERO256();
	}

	// 様々なビット演算に関する関数（本コードでは使われていないものも多数ある）
	// | 演算子
	const BitBoard operator | (const BitBoard& bb) const {
		BitBoard retval(OR256(data, bb.data));
		return retval;
	}
	// |= 演算子
	const BitBoard& operator |= (const BitBoard& bb) {
		data = OR256(data, bb.data);
		return *this;
	}
	// & 演算子
	const BitBoard operator & (const BitBoard& bb) const {
		BitBoard retval(AND256(data, bb.data));
		return retval;
	}
	// &= 演算子
	const BitBoard& operator &= (const BitBoard& bb) {
		data = AND256(data, bb.data);
		return *this;
	}
	// ^ 演算子
	const BitBoard operator ^ (const BitBoard& bb) const {
		BitBoard retval(XOR256(data, bb.data));
		return retval;
	}
	// ^= 演算子
	const BitBoard& operator ^= (const BitBoard& bb) {
		data = XOR256(data, bb.data);
		return *this;
	}
	// ^ (単項) 演算子
	const BitBoard operator ~() const {
		BitBoard retval(ANDNOT256(data, CMPEQ256(data, data)));
		return retval;
	}
	// == 演算子
	bool operator==(const BitBoard& bb) const {
		return (TESTC256(data, bb.data) && TESTC256(bb.data, data)) ? true : false;
	}
	// != 演算子
	bool operator!=(const BitBoard& bb) const {
		return (TESTC256(data, bb.data) && TESTC256(bb.data, data)) ? false : true;
	}
	// (~b1) & b2 を計算する 
	static BitBoard andnot(const BitBoard& b1, const BitBoard& b2) {
		BitBoard retval(ANDNOT256(b1.data, b2.data));
		return retval;
	}
	// 自身が bb に完全に含まれているかどうかを調べる
	bool isincluded(const BitBoard& bb) const {
		return (*this & bb) == *this;
	}
	// すべてのビットが 0 かどうかを計算する
	bool iszero() const {
		return TESTC256(SETZERO256(), data) ? true : false;
	}
	// = (代入) 演算子
	void operator=(const BitBoard& bb) {
		data = bb.data;
	}
	// (x, y) のビットを 1 にする（以下、ビッグエンディアンの場合はアドレスが変わるので変える必要があるはず）
	void set(const int x, const int y) {
		assert(0 <= x && x < BOARD_WIDTH && 0 <= y && y < BOARD_HEIGHT);
		*(reinterpret_cast<uint32_t *>(d8 + x * 3)) |= 1 << y;
	}
	// (x, y) のビットを 0 にする
	void reset(const int x, const int y) {
		assert(0 <= x && x < BOARD_WIDTH && 0 <= y && y < BOARD_HEIGHT);
		*(reinterpret_cast<uint32_t *>(d8 + x * 3)) &= 0xffffffff - (1 << y);
	}
	// (x, y) のビットが 1 の場合に true を返す
	bool isset(const int x, const int y) {
		assert(0 <= x && x < BOARD_WIDTH && 0 <= y && y < BOARD_HEIGHT);
		return (*(reinterpret_cast<uint32_t *>(d8 + x * 3)) & (1 << y)) ? true : false;
	}
	// x列目のデータを取得する(reinterpret_castを使うと const を入れることはできないようだ）
	uint32_t getx(const int x) {
		return *(reinterpret_cast<uint32_t *>(d8 + x * 3)) & 0x00ffffff;
	}
	// x列目のデータを設定する
	// data は 24 ビットのデータを超えないものとする
	void setx(const int x, const uint32_t data) {
		*(reinterpret_cast<uint32_t *>(d8 + x * 3)) = (*(reinterpret_cast<uint32_t *>(d8 + x * 3)) & 0xff000000) | data;
	}

	// ビットボードの内容をダンプする関数
	void dump() {
		for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				cerr << (isset(x, y) ? "O" : ".");
			}
			cerr << endl;
		}
		cerr << endl;
	}

	// ビットボード上の1のビット数を数える関数
	int pcount() const {
		return static_cast<int>(popcnt64(d64[0]) + popcnt64(d64[1]) + popcnt64(d64[2]) + popcnt64(d64[3]));
	}

	// 256ビットのデータ
	// union にすることで、8ビットごとにアクセスできるようにする（実際には24ビット単位でデータを取りたい）
	// また、64bit の popcnt でビット数を計算できるように d64 でアクセスできるようにしておく
	union {
		__m256i data;
		uint8_t d8[32];
		uint64_t d64[4];
	};
};

// init 関数でゲーム開始時に計算しておくテーブル
// インデックスを x, y 座標とした際に、
// (x, y) の周囲の8マスを表すビットボード
BitBoard around_bb_table[BOARD_WIDTH][BOARD_HEIGHT];
// (x, x列のビットパターン) の周囲8マスを表すビットボード。
// x列のビットパターンを示すことで、まとめて x列のビットが1の座標の周囲8マスを表すビットボードを計算できるようにするためのもの
BitBoard around_bb_x_table[BOARD_WIDTH][1 << BOARD_HEIGHT];
// ゲーム盤上のに存在可能なブロックの最大数。パックのブロックの最大数は4なのでそれを足す
constexpr int MAX_BLOCK = BOARD_WIDTH * LOAD_BOARD_HEIGHT + 4;
// 最大チェイン数。一回のチェインで最小2ブロックが消えるので、2で割る
constexpr int MAX_CHAIN = MAX_BLOCK / 2;
// チェインによるスコアのテーブル（作ったけど使ってない）
int chain_scoretable[MAX_CHAIN + 1];
// チェインによるお邪魔のテーブル
int chain_ojamatable[MAX_CHAIN + 1];
// スキルによるスコアのテーブル（作ったけど使ってない）
int skill_scoretable[MAX_BLOCK + 1];
// スキルによるお邪魔のテーブル
int skill_ojamatable[MAX_BLOCK + 1];

// 評価値に関するデータ
// PlayerData 内に組み込まれており、仕様メモリに影響するのでなるべくデータ量を小さくしたいので uint8_t などを使っている
struct HyoukaData {
	double total;				// 評価値の合計
#ifdef DEBUG
	double ojama;				// お邪魔の敵の数 - 味方の数
	double skillojama;			// スキルポイントによるお邪魔取得ポイント
	double eneskillojama;		// 敵のスキルポイントによるお邪魔
	double enedead;				// 敵が死亡する可能性が高い評価値
	double enemaydrop;			// 敵がお邪魔を落とした可能性がある場合についての評価値
	int16_t	limit;					// 上に積みすぎないようにするための評価値
	int16_t droppedojama;		// 敵のフィールドに落下した邪魔の数による評価値（-160～160の範囲をとるので、int16_t にしておく必要がある）
	uint8_t normalblocknum;		// 通常のブロックの数（最大値は190なのでuint8_tでOK)
	uint8_t enedeaddepth;		// 敵が死んた場合の深さ(0 の場合は死亡していない）
	int16_t skilldeleteblocknum;	// スキル発動で消せる(または消した）ブロック数
#endif
	uint8_t chain;				// ビーム探索時の評価における、最大深さにおけるチェーン数
	uint8_t chainxr;			// ビーム探索時において、最大連鎖が発生する x, r を x * 10 + r で表現する
	uint8_t maxchain;			// ビーム探索時の評価における、これまでの最大チェーン数
	uint8_t maxdepth;			// 最大深さ

	// 評価値の初期化。すべて0にする
	void clear() {
		memset(this, 0, sizeof(HyoukaData));
	}
	// 評価値のダンプ
	void dump() const {
#ifdef DEBUG
		cerr << "hyo " << total << " oj " << ojama << " do " << static_cast<int>(droppedojama) << " skoj " << skillojama << " eskoj " << eneskillojama << " sdel " << skilldeleteblocknum << " lim " << static_cast<int>(limit) << " edead " << enedead << " edeaddepth " << static_cast<int>(enedeaddepth) << " bn " << static_cast<int>(normalblocknum) << " emd " << enemaydrop <<  endl;
#endif
	}
};

// プレーヤーの情報を表す。使用メモリに大きく関係し、なるべくデータ量を少なくしたいので int8_t なども使っていく
struct PlayerInfo {
	// ゲーム盤を表すビットボードの数
	static constexpr int BB_SIZE = 20;
	// ゲーム盤の各マスにブロックが存在するかどうかを表すビットボード
	// インデックス値の意味は以下の通り
	// 0: 任意のブロックが存在するかどうか
	// 1～9: インデックスの番号のブロックが存在するかどうか
	// 10: お邪魔ブロックが存在するかどうか
	// 11～19: (インデックスの番号 - 10) のブロックが存在するマスの周囲8マス（消滅判定に使う）
	BitBoard bb[BB_SIZE];
	// 残り時間
	int32_t timeleft;
	// 各x座標の列のブロックの数
	uint8_t blocknum[BOARD_WIDTH];
	// ターン数
	int16_t turn;
	// これまでに相手に与えたお邪魔の総数と減らしたスキルの総数
	// まずありえないが、65536を超える場合があるので、int32_t にしておく
	int32_t getojama;
	// こちらは255を超えることはないのでuint8_t
	uint8_t getskillminus;
	// スキルによって相手に与えたお邪魔の数
	int16_t getskillojama;
	// この深さでスキルを使用したかどうか
	bool skillusedinthisdepth;
	// 探索において過去に一度でもスキルを使用したかどうか
	bool skillused;
	// スキルを使った、または10連鎖以上したターン
	int16_t skillusedturn;
	// estatus のスキル使用に関するインデックス番号
	uint8_t skillindex;
	// 敵がスキルを使用した可能性があるかどうか
	bool mayeneuseskill;
	// 敵のスキルで消せるブロック数
	uint8_t eneskilldeleteblocknum;
	// 敵のスキルを使わなかった場合のブロックの最小値
	uint8_t eneskillnotuseminblocknum;
	// お邪魔の最小値
	uint8_t minojamanum;
	// 敵のスキルを使わなかった場合のお邪魔の数の差異
	int16_t eneskillnotuseojamadiff;
	// 自分のフィールドに最初にお邪魔が降ってきたかどうか
	bool ojamadropped;
	// これまでにお邪魔が降ってきた探索深さを表すビット列（最下位ビットが深さ0を表す）
	uint8_t ojamadroppeddepthbit;
	// これまでに敵にお邪魔が降ってきた探索深さを表すビット列
	uint8_t eneojamadroppeddepthbit;
	// このターンのチェイン数
	uint8_t chain;
	// これまでの最大チェイン数
	uint8_t maxchain;
	// 味方と敵のお邪魔ブロックの数(40連鎖以上で65536を超えるので、念のためint32_tにしておく）
	int32_t ojama[2];
	// 味方と敵のスキルポイント
	int16_t skill[2];
	// スコア
	int32_t score;
	// 取った行動
	int8_t x, r;
	// 前の深さで取った行動（スレッドの場合は、pxは深さ0で行うxの行動を表す）
	int8_t px, pr;
	// 最初の深さで取った行動
	int8_t fx, fr;
	// 最初の深さのチェイン数
	int8_t fchain;
	// 敵と味方で、あと何段おじゃまが降ってきたら100%死亡するか
	uint8_t deadojamanum[2];
	// 敵がお邪魔を不確定だが落としたことにした回数
	int8_t enemayojamadroppedturnnum;
	// 敵がお邪魔を不確定だが落とした深さ
	int8_t enemayojamadroppeddepth;
	// 敵がお邪魔を落とせる状況で落とさなかったことにした回数
	int8_t enenotojamadroppedturnnum;
	// 敵がこれまでにおとした不確定なお邪魔の数
	int16_t enemayojamadropnum;
	// 敵がこれまで減らした不確定なスキルポイントの数
	int8_t enemayskillminusnum;
	// 敵の最初の深さでのフィールド内のお邪魔の数
	uint8_t firstenedroppedojamanum;
	// 前の深さの PlayerInfo
	PlayerInfo *prev;

	// 評価のデータ
	HyoukaData hyouka;

	// 全BitBoardの初期化
	void clearbb() {
		//for (int i = 0; i < BB_SIZE; i++) {
		//	bb[i].clear();
		//}
		// こっちのほうが速い
		memset(bb, 0, sizeof(BitBoard) * BB_SIZE);
	}

	// 盤面の情報のダンプ（デバッグ用）
	void dump_field(const bool displayhyouka = false) {
		int field[BOARD_WIDTH][BOARD_HEIGHT];
		for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				field[x][y] = 0;
			}
		}
		for (int b = 1; b <= 10; b++) {
			for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
				for (int x = 0; x < BOARD_WIDTH; x++) {
					if (issetbb(b, x, y)) {
						field[x][y] = b;
					}
				}
			}
		}

		if (displayhyouka) {
			hyouka.dump();
		}
		cerr << "Turn " << setw(5) << turn << endl;
		cerr << "do " << setw(3) << deadojamanum[0] / 10 << "/" << setw(3) << deadojamanum[1] / 10 << endl;
		cerr << "bl " << setw(3) << bb[0].pcount() - bb[10].pcount() << "/" << setw(3) << bb[0].pcount() << endl;
		cerr << "os " << setw(3) << bb[10].pcount() << "," << setw(3) << calcskilldeleteblocknum() << endl;
		cerr << "x " << setw(3) << static_cast<int>(x) << " r " << setw(2) << static_cast<int>(r) << endl;
		cerr << "px " << setw(2) << static_cast<int>(px) << " r " << setw(2) << static_cast<int>(pr) << endl;
		cerr << "oj " << setw(7) << getojama << endl;
		cerr << "ch " << setw(2) << static_cast<int>(chain);
		if (skillusedinthisdepth) {
			cerr << " S ";
		}
		else {
			cerr << " - ";
		}
		if (skillused) {
			cerr << " s";
		}
		else {
			cerr << " -";
		}
		cerr << endl;
		cerr << "sp " << setw(3) << skill[0] << "/" << setw(3) << skill[1] << endl;
		cerr << "oj " << setw(3) << ojama[0] << "/" << setw(3) << ojama[1] << endl;
		cerr << "od " << setw(3) << static_cast<int>(ojamadroppeddepthbit) << "/" << setw(3) << static_cast<int>(eneojamadroppeddepthbit) << endl << endl;
		for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				if (field[x][y] == 0) {
					if (y >= 16) {
						cerr << "!";
					}
					else {
						cerr << ".";
					}
				}
				else if (field[x][y] < 10) {
					cerr << field[x][y];
				}
				else {
					cerr << "X";
				}
			}
			cerr << endl;
		}
		cerr << endl;
		for (int x = 0; x < BOARD_WIDTH; x++) {
			if (blocknum[x] < 10) {
				cerr << static_cast<int>(blocknum[x]);
			}
			else {
				cerr << static_cast<char>('A' + blocknum[x] - 10);
			}
		}
		cerr << endl << endl;
	}
	// bb[i] の x, y をセットする
	void setbb(const int i, const int x, const int y) {
		bb[i].set(x, y);
	}
	// bb[i] の x, y をリセットする
	void resetbb(const int i, const int x, const int y) {
		bb[i].reset(x, y);
	}
	// bb[i] の x, y が存在するかをチェックする
	bool issetbb(const int i, const int x, const int y) {
		return bb[i].isset(x, y);
	}
	// bb[i] の x列目のデータに data をセットする
	void setbbx(const int i, const int x, const uint32_t data) {
		bb[i].setx(x, data);
	}
	// bb[i] の x列目のデータに data をセットする
	uint32_t getbbx(const int i, const int x) {
		return bb[i].getx(x);
	}

	// スキルによって消去されるブロック数の計算
	int calcskilldeleteblocknum() const {
		return ((bb[5] | bb[15]) & BitBoard::andnot(bb[10], bb[0])).pcount();
	}

	// お邪魔落下処理
	void ojamadrop(int depth) {
		// 自分のお邪魔落下処理
		if (ojama[0] >= 10) {
			// 自身のお邪魔を10減らす
			ojama[0] -= 10;
			// 各列に対してお邪魔ブロックの落下処理を行う
			for (int ox = 0; ox < BOARD_WIDTH; ox++) {
				// 列のお邪魔のブロック数を表す変数への参照
				// 任意のブロックを表す ox 列のビットボードにお邪魔ブロックを追加する
				setbb(0, ox, blocknum[ox]);
				// お邪魔ブロックを表す ox 列のビットボードにお邪魔ブロックを追加する
				setbb(10, ox, blocknum[ox]);
				// ox 列のブロック数を1増やす
				blocknum[ox]++;
			}
			// お邪魔ブロックが降ってきた木の深さを表すビットを更新する
			ojamadroppeddepthbit |= (1 << depth);
			// お邪魔が降ってきたことを表すフラグを立てる
			ojamadropped = true;
			// 致死量のお邪魔の数を減らす
			deadojamanum[0] -= 10;
		}
		else {
			ojamadropped = false;
		}
		// 敵のお邪魔落下処理
		// 敵のフィールドの処理は行わない（代わりにあらかじめ計算しておいた estatus の情報を後で使う）
		if (ojama[1] >= 10) {
			// 敵のお邪魔を減らす
			ojama[1] -= 10;
			// お邪魔が降ってきたことを表すフラグを立てる
			eneojamadroppeddepthbit |= (1 << depth);
			// 致死量のお邪魔の数を減らす
			deadojamanum[1] -= 10;
			eneskillnotuseminblocknum += 10;
			minojamanum += 10;
		}
	}

	// (bx, by) に隣接（斜めもあり）するブロックの塊を計算して blocksbb に格納する関数
	void searchblocks(BitBoard& blocksbb, BitBoard& checkedbb, const int bx, const int by, bool &candisappear) {
		// 左、右、下の盤外の場合は何もせず終了
		if (bx < 0 || bx >= BOARD_WIDTH || by < 0) {
			return;
		}
		// 上の死亡しない範囲での盤外の場合は、消える可能性があるので candisappear を true にして終了する
		else if (by >= LOAD_BOARD_HEIGHT) {
			candisappear = true;
			return;
		}
		// チェック済のマスの場合は何もせず終了する
		if (checkedbb.isset(bx, by)) {
			return;
		}
		// チェック済にする
		checkedbb.set(bx, by);
		// ブロックが存在しない場合は消える可能性があるので、 candisappear を true にして終了する
		if (!bb[0].isset(bx, by)) {
			candisappear = true;
			return;
		}
		blocksbb.set(bx, by);
		// 5のブロックが存在する場合は消える可能性があるので、 candisappear を true にする
		if (bb[5].isset(bx, by)) {
			candisappear = true;
		}
		// 周囲8マスに対してこの関数を再起呼び出しする
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				if (dx == 0 && dy == 0) {
					continue;
				}
				searchblocks(blocksbb, checkedbb, bx + dx, by + dy, candisappear);
			}
		}
	}

	// あと何個お邪魔を降らせれば確実に死ぬことを表す、deadojamanum を計算する
	void calcdeadojamanum() {
		Timer t;
		// お邪魔ブロックと、何があっても消えないブロックを合わせた BitBoard
		BitBoard ojamabb;
		// チェック済かどうかを表す BitBoard
		BitBoard checkedbb;
		// お邪魔ブロックの BitBoard をコピーする
		ojamabb = bb[10];
		// お邪魔ブロックの位置をチェック済とする
		checkedbb = ojamabb;
		// 下の行から一つずつマスを調べていく
		for (int by = 0; by < LOAD_BOARD_HEIGHT; by++) {
			for (int bx = 0; bx < BOARD_WIDTH; bx++) {
				// チェック済でない場合は、そこからつながるブロックの塊を調べる
				if (!checkedbb.isset(bx, by)) {
					// ブロックの塊を表す BitBoardの初期化
					BitBoard blocksbb;
					blocksbb.clear();
					// 消える可能性があるかどうか
					bool candisappear = false;
					searchblocks(blocksbb, checkedbb, bx, by, candisappear);
					// ブロックの塊の周囲がすべてお邪魔ブロックでふさがれており、内部に5のブロックがない場合は絶対消えない可能性がある
					if (!candisappear) {
						// 消えないのは、checkedbb の左右1列を含むすべての列で、by より下に消えないブロック以外のものが存在しない場合
						int minx = -1;
						int maxx;
						for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
							if (popcnt(blocksbb.getx(x2) > 0)) {
								if (minx == -1) {
									minx = x2;
								}
								maxx = x2;
							}
						}
						// 左右1列を含める
						minx--;
						maxx++;
						bool isojama = true;
						for (int x2 = minx; x2 <= maxx; x2++) {
							if (x2 < 0 || x2 >= BOARD_WIDTH) {
								continue;
							}
							if ((ojamabb.getx(x2) & ((1 << by) - 1)) != (1 << by) - 1) {
								isojama = false;
							}
						}
						//cerr << isojama << endl;
						if (isojama) {
							ojamabb |= blocksbb;
						}
					}
				}
			}
		}
		// 列のお邪魔に相当するブロックの最小値と最大値
		int maxojamanum = 0;
		int minojamanum = 100;
		for (int bx = 0; bx < BOARD_WIDTH; bx++) {
			int ojamanum = popcnt(ojamabb.getx(bx));
			if (maxojamanum < ojamanum) {
				maxojamanum = ojamanum;
			}
			if (minojamanum > ojamanum) {
				minojamanum = ojamanum;
			}
		}
		// 死亡確定のお邪魔の数は、ゲームおーばになる高さ - 列のお邪魔の最大値
		deadojamanum[0] = GAMEOVER_Y - maxojamanum;
		// 全ての列で同じ高さの場合は、さらに1引く
		if (minojamanum == maxojamanum) {
			deadojamanum[0]--;
		}
		deadojamanum[0] *= 10;
	}

	// 評価値の計算
	void calchyouka();

	// 敵が死亡した場合の評価値の計算
	void calcenedeadhyouka(int depth) {
		// 評価値は、敵が死んだ深さによって計算する
		// enedeaddepthによって敵が死んだかどうかを判定する（1以上の場合は死亡）
#ifdef DEBUG
		hyouka.enedeaddepth = depth;	
#endif
		hyouka.total = 1e20 - depth * 1e18;
	}

	// ビーム探索時で、スキル発動を目指す場合の評価値の計算
	void calcbeamhyouka_skill() {
		// スキルで消せるブロック数
		int16_t hyouka_skilldeleteblocknum = calcskilldeleteblocknum();
		int skillpoint = skill[0];
		// 80以上のばあいは80に補正
		if (skillpoint > 80) {
			skillpoint = 80;
		}
		// chain に skilldeleteblocknum を入れておくことにより、beamsearch での多様性を確保する。これOK?
		hyouka.chain = static_cast<uint8_t>(hyouka_skilldeleteblocknum);
		// MAX_CHAINを超えるとまずいのでチェックする
		if (hyouka.chain >= MAX_CHAIN) {
			hyouka.chain = MAX_CHAIN;
		}
#ifdef DEBUG
		hyouka.skilldeleteblocknum = hyouka_skilldeleteblocknum;
#endif
		// 評価値は、スキルで相手に落とせるお邪魔の数 * スキルポイント
		hyouka.total = hyouka_skilldeleteblocknum * skillpoint;
	}

	// ビーム探索時で、大連鎖を目指す場合の評価値の計算
	void calcbeamhyouka();

	// 自分の探索時のハッシュ値の計算 fnv_1 を使う
	uint64_t calc_hash()
	{
		static const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037U;
		static const uint64_t FNV_PRIME_64 = 1099511628211LLU;

		uint64_t hash = FNV_OFFSET_BASIS_64;
		// 相手の情報に関しては、スキルとお邪魔を考慮すればよい
		// turn は最大500なので9ビット必要（ここでは16ビットで計算）、skillは最大100なので8ビットで十分
		// ojama は 40連鎖以上すると 65536 以上降ってくるが、現実的にはありえなさそうなので、16ビットで計算する
		uint64_t val = turn | (skill[0] << 16) | (skill[1] << 24) | (static_cast<uint64_t>(ojama[0]) << 32) | (static_cast<uint64_t>(ojama[1]) << 48);
		// 1 ～ 9 とお邪魔の 10 の Bitboard に対する計算
		hash = (hash ^ val) * FNV_PRIME_64;
		for (int i = 1; i <= 10; ++i) {
			for (int j = 0; j < 4; j++) {
				hash = (hash ^ bb[i].d64[j]) * FNV_PRIME_64;
			}
		}

		return hash;
	}

	// 敵の探索の場合のハッシュ値の計算 fnv_1 を使う
	uint64_t calc_enemy_hash()
	{
		static const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037U;
		static const uint64_t FNV_PRIME_64 = 1099511628211LLU;

		uint64_t hash = FNV_OFFSET_BASIS_64;
		// turn は最大500なので9ビット必要、skillは最大100なので8ビットで十分
		// ojama は 40連鎖以上すると 65536 以上降ってくるが、現実的にはありえなさそうなので、16ビットで計算する
		// ojamadroppeddepthbit も考慮に入れる必要がある。これは今の所最大4ビットのデータ。turnは500までなので、10ビットにして余った6ビットをこれに割り当てる
		uint64_t val = turn | (skill[0] << 10) | (skill[1] << 18) | (static_cast<uint64_t>(ojama[0]) << 24) | (static_cast<uint64_t>(ojama[1]) << 40) | (static_cast<uint64_t>(ojamadroppeddepthbit) << 56);
		// 1 ～ 9 とお邪魔の 10 の Bitboard に対する計算
		hash = (hash ^ val) * FNV_PRIME_64;
		for (int i = 1; i <= 10; ++i) {
			for (int j = 0; j < 4; j++) {
				hash = (hash ^ bb[i].d64[j]) * FNV_PRIME_64;
			}
		}
		return hash;
	}

	// ビームサーチ時のハッシュ値の計算 fnv_1 を使う
	uint64_t calc_beam_hash()
	{
		static const uint64_t FNV_OFFSET_BASIS_64 = 14695981039346656037U;
		static const uint64_t FNV_PRIME_64 = 1099511628211LLU;

		uint64_t hash = FNV_OFFSET_BASIS_64;
		// ビーム時はお邪魔やスキルは考慮しないので、盤面以外はターンだけでよい
		uint64_t val = turn;
		hash = (hash ^ val) * FNV_PRIME_64;
		for (int i = 1; i <= 10; ++i) {
			for (int j = 0; j < 4; j++) {
				hash = (hash ^ bb[i].d64[j]) * FNV_PRIME_64;
			}
		}

		return hash;
	}

	// お邪魔の相殺チェック
	void checkojama() {
		// 両方とも正の場合
		if (ojama[0] > 0 && ojama[1] > 0) {
			// 大きいほうから小さいほうの値を引き、小さいほうを0とする
			if (ojama[0] > ojama[1]) {
				ojama[0] -= ojama[1];
				ojama[1] = 0;
			}
			else {
				ojama[1] -= ojama[0];
				ojama[0] = 0;
			}
		}
	}

	// スキルとお邪魔のチェック（連鎖による相手のスキルポイントの減少の処理も含む）
	void calcskillandojama() {
		if (chain >= 3) {
			// 相手のスキル減少値の計算（相手のスキル増加、増加あふれの処理の後に行う必要あり）
			int skillminus = 12 + chain * 2;
			// 相手のスキルを減らす
			skill[1] -= skillminus;
			// 累積相手のスキル減少値を増やす
			getskillminus += skillminus;
		}

		// スキルの範囲チェック
		for (int i = 0; i < 2; i++) {
			if (skill[i] < 0) {
				skill[i] = 0;
			}
			else if (skill[i] > 100) {
				skill[i] = 100;
			}
		}
		// お邪魔のチェック
		checkojama();
	}

	// ソートするための < 演算子の定義
	bool operator<(const PlayerInfo &pinfo) const
	{
		// 評価値を比較。降順にしたいので大きい場合にtrueとする
		return hyouka.total > pinfo.hyouka.total;
	};

	// ターン開始時に使う初期化処理
	void init(const int t) {
		// 全ビットボードの初期化
		clearbb();
		// 総獲得スコア、総獲得お邪魔、総獲得相手から減らすスキルポイントの初期化
		getojama = 0;
		getskillminus = 0;
		getskillojama = 0;
		// ターンの設定
		turn = t;
		// 行動を x = 0, r = 0 としておく
		x = 0;
		r = 0;
		// 味方と敵のお邪魔が降ってきたターンを表すビット列の初期化
		ojamadroppeddepthbit = 0;
		eneojamadroppeddepthbit = 0;
		// スキルを使ったかどうかを表す変数の初期化
		skillusedinthisdepth = false;
		skillused = false;
		skillusedturn = 0;
		skillindex = 0;
		mayeneuseskill = false;
		eneskilldeleteblocknum = 0;
		eneskillnotuseminblocknum = 0;
		eneskillnotuseojamadiff = 0;
		minojamanum = 0;
		enemayojamadroppedturnnum = 0;
		enenotojamadroppedturnnum = 0;
		enemayojamadropnum = 0;
		enemayojamadroppeddepth = 0;
		enemayskillminusnum = 0;
		maxchain = 0;
		prev = nullptr;
		hyouka.clear();
	}
};

// AIで使用するパラメータ
struct Parameter {
	// デフォルト設定
	// 自分と敵の読みの最大深さ
	int searchDepth = 4;
	int enesearchDepth = 4;
	// スレッドを使うかどうか
	bool usethread = false;
	// 置換表を使うかどうか
	bool usett = true;
	// ビーム探索の深さ
	int beamsearchDepth = 20;
	// ビーム探索の幅
	int beamsearchWidth = 5000;
	// ビーム探索の連鎖数毎の幅
	int beamsearchChainWidth = 1500;
	// 最初に組んだ大連鎖の行動に対して、相手に降らすお邪魔が beamignoreojamamax を超え、今回の行動での連鎖数が beamignorechain 以上の場合にビームサーチの行動を無視する値
	int beamIgnoreHyoukamax = 40;
	int beamIgnoreHyoukamin = -20;
	int beamIgnoreHyoukadiff = 15;
	int nextbeamIgnoreHyoukadiff = 20;
	//// 自分に降ってくるお邪魔がこの値を超えたらビームサーチの行動を維持する値
	//int beamNotIgnoreOjamanum = 50;
	// ビームサーチでこの連鎖数を超えたら探索を中止する値（負の場合はbeamSearchDepthまで必ず実行する)
	int beamChainmax = -1;
	int beamChainmax1 = -1;
	int beamChainmaxminus = 1;
	int beamChainmax2 = 14;
	// ビームサーチで上記の連鎖数を超えた場合に、その連鎖数の候補のみを残して続けるかどうか
	bool stopbeamChainmax = false;
	// ビームサーチでのxの範囲
	int beamminx = 0;
	int beammaxx = 8;
	// ビームサーチで見つかった初手の行動
	int beamx = -2;
	int beamr = -1;
	// 連鎖確認のデバッグ用で使う。2手目の行動
	int beamx2 = -2;
	int beamr2 = -1;
	//
	bool showmyanalyze = false;
	//int maxskillblocknum = 50;
	// デバッグ表示をするかどうか
	bool debugmsg = false;
	// ターンのデバッグ表示をするかどうか
	bool turndebugmsg = false;
	// 連続するゲームデータからビームサーチの結果のみを計算して表示する
	bool checkbeamchain = false;
	// 特定のゲームのみ実行するかどうか。正の値のときのみ有効（デバッグ用）
	int checkgame = -1;
	// 時間制限を無視する（デバッグ用）
	bool ignoretime = false;
	// 味方の estatus も計算する（デバッグ用）
	bool calcmyestatus = false;
	// 特定のターンのみ実行するかどうか。正の値のときのみ有効（デバッグ用）
	int checkturn = -1;
	// 次の連鎖を組むかどうか
	bool nextchain = false;
	int nextbeamsearchWidth = 100;
	int nextbeamsearchChainWidth = 50;
	int nextfirsttimelimit = 500;
	int nextbeamsearchDepth = 10;
	// 次の連鎖で見つかった連鎖数がこの値以下の場合に、連鎖を目指すか、スキル発動を目指すかを判断する
	int nextbeamsearchminChain = 9;

	// こちらが初手で連鎖可能な場合に、敵の浅い深さでの連鎖数を計算する際のビーム探索のパラメータ
	int enebeamsearchWidth = 1600;
	int enebeamsearchChainWidth = 400;
	int enebeamsearchDepth = 5;
	int enebeamsearchlimit = 1000;

	// 敵の連鎖を計算する際のビーム探索のパラメータ（深さは nextbeamsearchDepth と共通）
	int nextenebeamsearchWidth = 750;
	int nextenebeamsearchChainWidth = 250;
	int nextenebeamsearchlimit = 500;

	// 敵がお邪魔を落としてきた場合のビーム探索のパラメータ
	int droppedbeamsearchWidth = 350;
	int droppedbeamsearchChainWidth = 100;
	int droppedbeamsearchDepth = 7;
	int droppedbeamsearchlimit = 500;

	// スキル発動のビーム探索のパラメータ
	int skillbeamsearchWidth = 1000;
	int skillbeamsearchChainWidth = 200;
	int skillbeamsearchDepth = 10;
	int skillbeamsearchlimit = 500;

	// スキルが使えない場合の致死量の高さの列に対する評価値
	int hyouka_limit = -200;
	// 1ターン目のタイムリミット
	int firsttimelimit = 19000;
	int beamtimelimit = 19250;
	bool calceneskill = true;
	// ビームの評価で、過去の最高の評価値を採用するかどうか
//	bool usebeamhyoukamul = true;
//	double beamhyoukamul = 1.0;
//	double firstbeamhyoukamul = 1.0;
//	double nextbeamhyoukamul = 1.0;
	double enemaydrophyouka = -5.0;
	double enenotdrophyouka = 5.0;
	int firstminchainnum = 10;
	int secondminchainnum = 9;
	double firstminchainminus = -100;
	double secondminchainminus = -50;
	// search()で評価値を計算する際に、リーフノードで最高の評価値を計算する場合はfalse（この場合、関数aiの返り値に意味はない）
	// true にした場合は、ai関数はそのノードの評価値を返し、depth=0において、最高評価値を計算する
	bool ai2 = false;
	// true にした場合は、search()の探索時に、敵がお邪魔を降らせることが可能な場合、降らせなかった場合の行動も考慮する（低いほうの評価値を採用する）
	// ai3 は ai2 が true の場合のみ有効
	bool ai3 = false;
	// 各ターンの状況で、初手でチェーンを発動した場合の状況をチェックするかどうか
	bool checkfirstchain = true;
	bool firstskillmode = false;

	bool showchain = false;
	// パラメータのパース
	void parseparam(int argc, char *argv[]) {
		// パラメータの解釈
		for (int i = 1; i < argc; i++) {
			if (strcmp(argv[i], "-ut") == 0 && argc > i) {
				usethread = true;
			}
			if (strcmp(argv[i], "-fsm") == 0 && argc > i) {
				firstskillmode = true;
			}
			if (strcmp(argv[i], "-ai2") == 0 && argc > i) {
				ai2 = true;
			}
			if (strcmp(argv[i], "-ai3") == 0 && argc > i) {
				ai3 = true;
			}
			if (strcmp(argv[i], "-ncfc") == 0 && argc > i) {
				checkfirstchain = false;
			}
			if (strcmp(argv[i], "-d") == 0 && argc > i + 1) {
				i++;
				searchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-ed") == 0 && argc > i + 1) {
				i++;
				enesearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bd") == 0 && argc > i + 1) {
				i++;
				beamsearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bw") == 0 && argc > i + 1) {
				i++;
				beamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bcw") == 0 && argc > i + 1) {
				i++;
				beamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nbw") == 0 && argc > i + 1) {
				i++;
				nextbeamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nbcw") == 0 && argc > i + 1) {
				i++;
				nextbeamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nbd") == 0 && argc > i + 1) {
				i++;
				nextbeamsearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nbminc") == 0 && argc > i + 1) {
				i++;
				nextbeamsearchminChain = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-ebw") == 0 && argc > i + 1) {
				i++;
				enebeamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-ebcw") == 0 && argc > i + 1) {
				i++;
				enebeamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-ebd") == 0 && argc > i + 1) {
				i++;
				enebeamsearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-ebt") == 0 && argc > i + 1) {
				i++;
				enebeamsearchlimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nebw") == 0 && argc > i + 1) {
				i++;
				nextenebeamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nebcw") == 0 && argc > i + 1) {
				i++;
				nextenebeamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nebt") == 0 && argc > i + 1) {
				i++;
				nextenebeamsearchlimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-dbw") == 0 && argc > i + 1) {
				i++;
				droppedbeamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-dbcw") == 0 && argc > i + 1) {
				i++;
				droppedbeamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-dbd") == 0 && argc > i + 1) {
				i++;
				droppedbeamsearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-dbt") == 0 && argc > i + 1) {
				i++;
				droppedbeamsearchlimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sbw") == 0 && argc > i + 1) {
				i++;
				skillbeamsearchWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sbcw") == 0 && argc > i + 1) {
				i++;
				skillbeamsearchChainWidth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sbd") == 0 && argc > i + 1) {
				i++;
				skillbeamsearchDepth = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sbt") == 0 && argc > i + 1) {
				i++;
				skillbeamsearchlimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bihmax") == 0 && argc > i + 1) {
				i++;
				beamIgnoreHyoukamax = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bihmin") == 0 && argc > i + 1) {
				i++;
				beamIgnoreHyoukamin = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bihd") == 0 && argc > i + 1) {
				i++;
				beamIgnoreHyoukadiff = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nbihd") == 0 && argc > i + 1) {
				i++;
				nextbeamIgnoreHyoukadiff = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bcm") == 0 && argc > i + 1) {
				i++;
				beamChainmax1 = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bcm-") == 0 && argc > i + 1) {
				i++;
				beamChainmaxminus = stoi(argv[i], NULL, 10);
				if (beamChainmaxminus > 0) {
					beamChainmaxminus = 0;
				}
			}
			else if (strcmp(argv[i], "-bcm2") == 0 && argc > i + 1) {
				i++;
				beamChainmax2 = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bx") == 0 && argc > i + 1) {
				i++;
				beamx = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-br") == 0 && argc > i + 1) {
				i++;
				beamr = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bx2") == 0 && argc > i + 1) {
				i++;
				beamx2 = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-br2") == 0 && argc > i + 1) {
				i++;
				beamr2 = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sbcm") == 0 && argc > i) {
				stopbeamChainmax = true;
			}
			else if (strcmp(argv[i], "-bminx") == 0 && argc > i + 1) {
				i++;
				beamminx = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-bmaxx") == 0 && argc > i + 1) {
				i++;
				beammaxx = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-fmcn") == 0 && argc > i + 1) {
				i++;
				firstminchainnum = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-fmcm") == 0 && argc > i + 1) {
				i++;
				firstminchainminus = stof(argv[i], NULL);
			}
			else if (strcmp(argv[i], "-smcn") == 0 && argc > i + 1) {
				i++;
				secondminchainnum = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-smcm") == 0 && argc > i + 1) {
				i++;
				secondminchainminus = stof(argv[i], NULL);
			}
			else if (strcmp(argv[i], "-hl") == 0 && argc > i + 1) {
				i++;
				hyouka_limit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-emdh") == 0 && argc > i + 1) {
				i++;
				enemaydrophyouka = stof(argv[i], NULL);
			}
			else if (strcmp(argv[i], "-endh") == 0 && argc > i + 1) {
				i++;
				enenotdrophyouka = stof(argv[i], NULL);
			}
			else if (strcmp(argv[i], "-nc") == 0 && argc > i) {
				nextchain = true;
			}
			else if (strcmp(argv[i], "-nes") == 0 && argc > i) {
				calceneskill = false;
			}
			else if (strcmp(argv[i], "-cb") == 0 && argc > i) {
				checkbeamchain = true;
			}
			else if (strcmp(argv[i], "-sma") == 0 && argc > i) {
				showmyanalyze = true;
			}
			else if (strcmp(argv[i], "-m") == 0 && argc > i) {
				debugmsg = true;
				turndebugmsg = true;
			}
			else if (strcmp(argv[i], "-tm") == 0 && argc > i) {
				turndebugmsg = true;
			}
			else if (strcmp(argv[i], "-it") == 0 && argc > i) {
				ignoretime = true;
			}
			else if (strcmp(argv[i], "-es") == 0 && argc > i) {
				calcmyestatus = true;
			}
			else if (strcmp(argv[i], "-ftl") == 0 && argc > i + 1) {
				i++;
				firsttimelimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-btl") == 0 && argc > i + 1) {
				i++;
				beamtimelimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-nftl") == 0 && argc > i + 1) {
				i++;
				nextfirsttimelimit = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-g") == 0 && argc > i + 1) {
				i++;
				checkgame = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-t") == 0 && argc > i + 1) {
				i++;
				checkturn = stoi(argv[i], NULL, 10);
			}
			else if (strcmp(argv[i], "-sc") == 0 && argc > i) {
				showchain = true;
			}
			else if (strcmp(argv[i], "-tt") == 0 && argc > i + 1) {
				i++;
				if (stoi(argv[i], NULL, 10) == 0) {
					usett = false;
				}
			}
		}
		if (turndebugmsg) {
			dump();
		}
	}

	// パラメータのダンプ
	void dump() const {
#ifdef DEBUG
		cerr << "Param: ai2 " << ai2 << " ai3 " << ai3 << " use thread " << usethread << " depth " << searchDepth << " enedepth " << enesearchDepth << " firsttimelimit " << firsttimelimit << " btimelimit " << beamtimelimit << " eneskill " << calceneskill << " checkfirstchain " << checkfirstchain << " firstskillmode " << firstskillmode << endl;
		cerr << "hyouka: limit " << hyouka_limit << " enemaydrop " << enemaydrophyouka << " enenotdrop " << enenotdrophyouka << " mincnum1 " << firstminchainnum << " minus " << firstminchainminus << " mincnum2 " << secondminchainnum << " minus " << secondminchainminus << endl;
		cerr << "beam: normal depth " << beamsearchDepth << " beam width " << beamsearchWidth << " chain width " << beamsearchChainWidth << " chainmax " << beamChainmax1 << " chainmax2 " << beamChainmax2 << " chainmaxminus " << beamChainmaxminus << " stopchainmax " << stopbeamChainmax << endl;
		cerr << "next depth " << nextbeamsearchDepth << " width " << nextbeamsearchWidth << " chain width " << nextbeamsearchChainWidth << " min chain " << nextbeamsearchminChain << " next timelimit " << nextfirsttimelimit << endl;
		cerr << "nextene width " << nextenebeamsearchWidth << " chain width " << nextenebeamsearchChainWidth << " timelimit " << nextenebeamsearchlimit << endl;
		cerr << "ene depth " << enebeamsearchDepth << " width " << enebeamsearchWidth << " chain width " << enebeamsearchChainWidth << " timelimit " << enebeamsearchlimit << endl;
		cerr << "dropped depth " << droppedbeamsearchDepth << " width " << droppedbeamsearchWidth << " chain width " << droppedbeamsearchChainWidth << " timelimit " << droppedbeamsearchlimit << endl;
		cerr << "skill depth " << skillbeamsearchDepth << " width " << skillbeamsearchWidth << " chain width " << skillbeamsearchChainWidth << " timelimit " << skillbeamsearchlimit << endl;
		cerr << "beamIgnoreHyouka: max " << beamIgnoreHyoukamax << " min " << beamIgnoreHyoukamin << " diff " << beamIgnoreHyoukadiff << " next diff " << nextbeamIgnoreHyoukadiff << endl;
		cerr << "beamminx " << beamminx << " beammaxx " << beammaxx << " nextchain " << nextchain << endl;
		cerr << "usett " << usett << " debugmsg " << debugmsg << " turndebugmsg " << turndebugmsg << " ignoretime " << ignoretime << " calcmyestatus " << calcmyestatus << " checkgame " << checkgame << " checkturn " << checkturn << " beamx " << beamx << " beamr " << beamr << " showchain " << showchain << endl;
#endif
	}
};

// ゲームに関する情報
struct GameInfo {
	// ターン数
	int turn;
	// 敵と味方の情報
	PlayerInfo pinfo[2];

	// ゲーム開始からの時間
	Timer t;
	// 最初に行った大連鎖を狙うためのビーム探索の結果を使用できるかどうか
	bool beamflag;
	// ビームサーチの結果得られた最善手する手
	int beamx, beamr;
	// スキルモードかどうか
	bool skillmode = false;
	// 初手でこの連鎖以上の連鎖を行ったときに、評価値にfirstchainpenaltyを与える
	// 相手がより大きな連鎖でカウンターしてくることがわかっている場合にペナルティを与える
	int maxfirstchain;
	double firstchainpenalty;
	// 初手でこの行動をとった場合に与えるボーナス
	// 初手で大連鎖を行った結果、相手がそれ以上の連鎖で返してこないことが分かっている場合にボーナスを与える
	int firstx, firstr;
	double firstbonus;

	// 敵の思考かどうか
	bool isenemy;

	// 探索深さ
	int searchDepth;
	// 自分の探索深さ、敵の探索深さ
	int mysearchDepth, enesearchDepth;

	// parameter.ai2 が true の場合、この深さから探索を開始したことを表す
	// （ai2depthが1以上の場合は、その深さの最善手を知るためのデバッグ用の処理を行っている）
	int ai2depth;

	// 関数 search() を呼び出した回数（デバッグ用）
#ifdef DEBUG
	int aicount;
#endif

	// ビーム探索の深さ、幅、チェイン毎の幅
	int beamDepth;
	int beamWidth;
	int beamChainWidth;

	// 自分と敵が、最初の状況で行う事が可能な最大チェーン数
	int myfirstmaxchain, enefirstmaxchain;

	// ビーム探索でみつかった最大チェイン数
	int maxchaindepth;
	// ビーム探索で見つかった、各チェイン数を最初に達成した深さ
	int eachchaindepth[MAX_CHAIN + 1];
	// 自身の盤面に対してビームサーチを行った場合、eachchaindepth の情報をここにコピーする
	int myeachchaindepth[MAX_CHAIN + 1];
	// ビーム探索で見つかった自身の最大チェーン数
	int mymaxchain;
	// 敵の盤面に対してビームサーチを行った場合、eachchaindepth の情報をここにコピーする
	int eneeachchaindepth[MAX_CHAIN + 1];
	// ビーム探索で見つかった敵の最大チェーン数
	int enemaxchain;
	// ビーム探索時にタイムアウトしたかどうか
	bool istimeout;

	// 最初の大連鎖を狙うビーム探索を行うかどうか
	bool firstbeamcheck = true;

	// ビーム探索時に、parameter.beamchainmax 以上の連鎖を初めて達成した深さ
	int beamchainmaxdepth;

	// 各深さにおいて、初手に hyoukabonusx, hyoukabonusr を取り、その深さで敵がお邪魔を落としてきた場合に
	// hyoukabonus[depth] のボーナスを評価値に加算する
	// ビームサーチで得られた探索の行動をとった場合に、相手がお邪魔を降らせてきた場合にカウンターできるかどうかを
	// 調べ、カウンターできる場合はボーナスを評価値に加算する
	int hyoukabonusx;
	int hyoukabonusr;
	double hyoukabonus[MAX_DEPTH + 1];

	// ビーム探索時に初手が ginfo.beamx, ginfo.beamr の行動のみをチェックするかどうか
	bool checkonlybeamxr;

	// ターン毎のブロックの情報
	// ブロックの番号を格納する変数
	// 　インデックスは順に、ターン数、回転数、x座標、y座標とする
	//   計算のしやすさを考えて、y 座標が 0 の位置のブロックが存在しない場合は、y座標が1のブロックを下にずらす
	int blockinfo[MAX_TURN][4][2][2];
	
	// ゲーム開始時の情報を標準入力から読み込む
	// ゲーム開始時の情報が正しく読み込めなかった場合は、"END"まで情報を読み込んで、false を返す
	bool read_start_info() {
		string dummy;
		// 各ターンのブロックの情報を読み込む
		for (int i = 0; i < MAX_TURN; i++) {
			// まず、単純にブロックの情報を順に blockinfo に読み込む
			for (int y = 1; y >= 0; y--) {
				for (int x = 0; x < 2; x++) {
					if (cin.eof()) {
						return false;
					}
					cin >> blockinfo[i][0][x][y];
				}
			}
			if (i == 0) {
				t.restart();
			}
			cin >> dummy;
			// ここで、"END"が読み込まれていない場合は、ゲーム開始時の情報ではないので、ENDがくるまで情報を読み込んで、falseを返す
			if (dummy != "END") {
				while (true) {
					cin >> dummy;
					if (dummy == "END") {
						return false;
					}
				}
			}
			// ブロックを回転させる
			for (int r = 1; r < 4; r++) {
				blockinfo[i][r][0][0] = blockinfo[i][r - 1][1][0];
				blockinfo[i][r][0][1] = blockinfo[i][r - 1][0][0];
				blockinfo[i][r][1][1] = blockinfo[i][r - 1][0][1];
				blockinfo[i][r][1][0] = blockinfo[i][r - 1][1][1];
			}
			// y座標が0のブロックが0の場合は、下にずらす
			for (int r = 0 ; r < 4; r++) {
				for (int x = 0; x < 2; x++) {
					if (blockinfo[i][r][x][0] == 0) {
						blockinfo[i][r][x][0] = blockinfo[i][r][x][1];
						blockinfo[i][r][x][1] = 0;
					}
				}
			}
		}
		return true;
	}

	// ターンの情報を標準入力から読み込む
	// ターンの情報が正しく読み込めなかった場合は false を返す
	bool read_turn_info() {
		// これがないとなぜか途中で固まることがある。謎
#ifdef DEBUG
		cerr << endl;
#endif
		string dummy;
		cin >> turn;
		// 
		if (cin.eof()) {
			return false;
		}
		for (int i = 0; i < 2; i++) {
			pinfo[i].init(turn);
			cin >> pinfo[i].timeleft >> pinfo[i].ojama[0] >> pinfo[i].skill[0] >> pinfo[i].score;
			for (int y = LOAD_BOARD_HEIGHT - 1; y >= 0; y--) {
				for (int x = 0; x < BOARD_WIDTH; x++) {
					int fnum;
					cin >> fnum;
					if (fnum > 11) {
						return false;
					}

					if (fnum == 11) {
						fnum = 10;
					}
					if (fnum > 0) {
						pinfo[i].setbb(0, x, y);
						pinfo[i].setbb(fnum, x, y);
						if (fnum < 10) {
							pinfo[i].bb[fnum + 10] |= around_bb_table[x][y];
						}
					}
				}
			}
			cin >> dummy;
			if (dummy != "END") {
				return false;
			}
			// 各行のブロック数を計算する
			for (int x = 0; x < BOARD_WIDTH; x++) {
				pinfo[i].blocknum[x] = popcnt(pinfo[i].bb[0].getx(x));
			}
		}
		return true;
	}

	// フィールドのダンプ
	void dump_field() {
#ifdef DEBUG
		cerr << "Turn " << turn << endl;
		pinfo[0].dump_field();
#endif
	}
};

// 敵の情報
struct EnemyStatus {
	// チェインしたかどうか
	bool ischain;
	// 累積相手に降らすお邪魔数
	uint16_t maxojama;
	// 累積相手から減らすスキルポイント
	int16_t maxskillminus;
//	// 累積取得スキルポイント
//	int maxskill;
	// この深さの最大チェイン数
	uint8_t maxchain;
	// この深さの最大スキルによるお邪魔発生数
	int16_t maxskillojama;
	// この深さの最大チェイン数が実現した際の行動
	int16_t maxchainx, maxchainr;
	// この状況でスキルで消せる最大ブロック数
	uint8_t skilldeleteblocknum;
	// この状況での最小ブロック数（お邪魔含む）
	uint8_t minblocknum;
	// この状況での最小お邪魔ブロック数
	uint8_t minojamanum;
	// この状況で死亡するかどうか
	bool isdead;

	// 初期化
	void clear() {
		ischain = false;
		maxojama = 0;
		maxskillojama = 0;
		maxskillminus = 0;
		maxchain = 0;
		maxchainx = -2;
		maxchainr = 0;
		minblocknum = 200;
		minojamanum = 200;
		skilldeleteblocknum = 0;
		isdead = true;
	}
	// ダンプ
	void dump() const {
#ifdef DEBUG
		if (isdead) {
			cerr << endl;
		}
		else {
			cerr << "ch " << (ischain ? "O" : "X") << " " << setw(2) << static_cast<int>(maxchain) << " x " << setw(2) << maxchainx << " r " << maxchainr << " so " << setw(3) << maxskillojama << " oj " << setw(4) << maxojama << " s- " << setw(3) << maxskillminus << " skdelbnum " << setw(3) << static_cast<int>(skilldeleteblocknum) << " minblnum " << setw(3) << static_cast<int>(minblocknum) << " minojnum " << setw(3) << static_cast<int>(minojamanum) << endl;
		}
#endif
	}
};

// 敵の行動の状態。インデックスの表す状態において、EnemyStatusが表す最大の状態を表す
// インデックスx, y, zの意味はそれぞれ以下の通り
// x: 深さ zの行動でスキルを使ったかどうか。0:過去に一度も使っていない 1:使っていないが過去に使った 2:使った 3: 1の2の最大値 4: 0と2の最大値
// y: 最下位ビットから順に深さ0,1...を表す。その深さでお邪魔が降ってきたかどうか
// z: 敵の深さ z - 1 の行動の結果のデータ(z=0の場合は、ターン開始時の状況を表す）
constexpr int ESTATUS_NUM = 5;
struct EnemyStatusData {
	EnemyStatus data[ESTATUS_NUM][1 << MAX_DEPTH][MAX_DEPTH + 1];

	// 初期化
	void clear(const int searchDepth) {
		for (int i = 0; i < ESTATUS_NUM; i++) {
			for (int j = 0; j < (1 << searchDepth); j++) {
				for (int k = 0; k <= searchDepth; k++) {
					data[i][j][k].clear();
				}
			}
		}
	}
};

// ターンのデータ
struct TurnData {
	// 初期化にかかった時間
	int inittime;
	// 自分のAIで消費した時間
	int mytime[MAX_TURN];
	// その合計
	int totalmytime;
	// その最大値
	int maxmytime;
	// 敵の行動のAIで消費した時間
	int enetime[MAX_TURN];
	// その合計
	int totalenetime;
	// その最大値
	int maxenetime;
	int beamtime;
	int beamainum;
	// 関数 ai を呼んだ回数
	int ainum[MAX_TURN];
	// その合計
	int totalainum;
	double mytimetotal[MAX_DEPTH];
	double mytimecount[MAX_DEPTH];
	double enetimetotal[MAX_DEPTH];
	double enetimecount[MAX_DEPTH];

	TurnData() : inittime(0), totalmytime(0), totalenetime(0), totalainum(0), maxmytime(0), maxenetime(0) {
		for (int i = 0; i < MAX_DEPTH; i++) {
			mytimetotal[i] = mytimecount[i] = enetimetotal[i] = enetimecount[i] = 0;
		}
	}
	void dump(const int turn) const {
#ifdef DEBUG
		cerr << "inittime " << inittime << " mytime " << totalmytime << " enetime " << totalenetime << " time " << inittime + totalmytime + totalenetime << " maxmytime " << maxmytime << " maxenetime " << maxenetime << " totalainum " << totalainum << endl;
		cerr << "beamtime " << beamtime << " beamainum " << beamainum << endl;
		cerr << "totaltime " << inittime + totalmytime + totalenetime + beamtime << " totalainum " << beamainum + totalainum << endl;
		cerr << "ainum/s " << (beamainum + totalainum) / (beamtime + totalmytime + totalenetime) * 1000 << endl;
		for (int t = 0; t <= turn; t++) {
			cerr << "Turn" << setw(3) << t << " mytime " << setw(5) << mytime[t] << " enetime " << setw(5) << enetime[t] << setw(10) << ainum[t] << endl;
		}
		cerr << "average think time." << endl;
		for (int i = 0; i < MAX_DEPTH; i++) {
			cerr << "depth " << i << " my " << setw(5) << mytimetotal[i] << "/" << setw(5) << mytimecount[i] << " ";
			if (mytimecount[i] == 0) {
				cerr << "----- ms ";
			}
			else {
				cerr << setw(5) << mytimetotal[i] / mytimecount[i] << " ms ";
			}
			cerr << " ene " << setw(5) << enetimetotal[i] << "/" << setw(5) << enetimecount[i] << " ";
			if (enetimecount[i] == 0) {
				cerr << "----- ms ";
			}
			else {
				cerr << setw(5) << enetimetotal[i] / enetimecount[i] << " ms ";
			}
			cerr << endl;
		}
#endif
	}
	void setbeam(const int time, const int anum) {
#ifdef DEBUG
		beamtime = time;
		beamainum = anum;
#endif
	}
	void set(const int turn, const int mtime, const int etime, const int anum, const int mdepth, const int edepth) {
#ifdef DEBUG
		mytime[turn] = mtime;
		enetime[turn] = etime;
		ainum[turn] = anum;
		mytimetotal[mdepth] += mtime;
		mytimecount[mdepth]++;
		enetimetotal[edepth] += etime;
		enetimecount[edepth]++;
		if (maxmytime < mtime) {
			maxmytime = mtime;
		}
		if (maxenetime < etime) {
			maxenetime = etime;
		}
		totalmytime += mtime;
		totalenetime += etime;
		totalainum += anum;
#endif
	}
};

