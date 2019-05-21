#include "codevsreborn.hpp"
#include "tt.hpp"

// 乱数関連。使わないのでコメントアウト
//mt19937 mt(20190405);
//uniform_real_distribution<> rnd_01(0, 1.0);

// ゲームに関連する情報を格納する変数
GameInfo ginfo;

// AIの思考に関するパラメータ
Parameter parameter;

// スレッド使用時の排他制御に使うミューテックス
std::mutex mtx;

// 置換表
TT_TABLE tt;

// AIの行動の結果を出力する関数
void output(int bestx, int bestr) {
	// スキル使用の場合
	if (bestx == -1) {
		cout << "S" << endl;
	}
	// スキルを使用しない場合
	else {
		cout << bestx << " " << bestr << endl;
	}
	cout.flush();
}

// PlayerInfo のフィールドを横に並べて表示する
// （besthistory を並べて表示するための関数）
// PlayerInfo の dump_field の複数版
// 引数：
//   pinfos: PlayerInfoの配列のポインタ
//   depth: pinfos の配列の数 - 1
//   isprev: trueの場合は、並べたPlayerInfoがビーム探索によってつながっている場合を表す
void dump_fields(PlayerInfo *pinfos, int depth, bool isprev=false) {
	// 各PlayerInfoのフィールドの情報を格納する変数
	int field[MAX_BEAM_DEPTH + 1][BOARD_WIDTH][BOARD_HEIGHT];
	// field の情報の設定
	for (int i = 0; i <= depth; i++) {
		for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				field[i][x][y] = 0;
			}
		}
		for (int b = 1; b <= 10; b++) {
			for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
				for (int x = 0; x < BOARD_WIDTH; x++) {
					if (pinfos[i].issetbb(b, x, y)) {
						field[i][x][y] = b;
					}
				}
			}
		}
	}

	cerr << endl;
	// 間のスペース
	string spaces = "   ";
	// 深さの表示
	for (int i = 0; i <= depth; i++) {
		cerr << "depth " << setw(4) << i << spaces;
	}
	cerr << endl;
	// 自分の残り思考時間
	for (int i = 0; i <= depth; i++) {
		cerr << "ti " << setw(7) << pinfos[i].timeleft << spaces;
	}
	cerr << endl;
	for (int i = 0; i <= depth; i++) {
		cerr << "turn " << setw(5) << pinfos[i].turn << spaces;
	}
	cerr << endl;
	// 致死量のお邪魔の数
	for (int i = 0; i <= depth; i++) {
		cerr << "do " << setw(3) << pinfos[i].deadojamanum[0] / 10 << "/" << setw(3) << pinfos[i].deadojamanum[1] / 10 << spaces;
	}
	cerr << endl;
	// 自分のお邪魔以外のブロック数、全ブロック数
	for (int i = 0; i <= depth; i++) {
		cerr << "bl " << setw(3) << pinfos[i].bb[0].pcount() - pinfos[i].bb[10].pcount() << "/" << setw(3) << pinfos[i].bb[0].pcount() << spaces;
	}
	cerr << endl;
	// 自分のお邪魔のブロック数、スキルで消えるブロック数
	for (int i = 0; i <= depth; i++) {
		cerr << "os " << setw(3) << pinfos[i].bb[10].pcount() << "/" << setw(3) << pinfos[i].calcskilldeleteblocknum() << spaces;
	}
	cerr << endl;
	// 相手に降らせるお邪魔の合計の表示
	for (int i = 0; i <= depth; i++) {
		cerr << "oj " << setw(3) << pinfos[i].getojama << "/" << setw(3) << static_cast<int>(pinfos[i].minojamanum) << spaces;
	}
	cerr << endl;
	// 一つ前の深さの連鎖数とスキルを使用したかどうか
	for (int i = 0; i <= depth; i++) {
		cerr << "ch " << setw(2) << static_cast<int>(pinfos[i].chain) << "/" << setw(2) << static_cast<int>(pinfos[i].maxchain) << " ";
		if (pinfos[i].skillusedinthisdepth) {
			cerr << "S";
		}
		else if (pinfos[i].skillused) {
			cerr << "s";
		}
		else {
			cerr << "-";
		}
		cerr << spaces;
	}
	cerr << endl;
	// 自分と相手のスキルポイント
	for (int i = 0; i <= depth; i++) {
		cerr << "sp " << setw(3) << pinfos[i].skill[0] << "/" << setw(3) << pinfos[i].skill[1] << spaces;
	}
	cerr << endl;
	// 自分と相手のお邪魔の数
	for (int i = 0; i <= depth; i++) {
		cerr << "oj " << setw(3) << pinfos[i].ojama[0] << "/" << setw(3) << pinfos[i].ojama[1] << spaces;
	}
	cerr << endl;
	// 自分と相手のお邪魔が降ってきたターンをビットで表した数値
	for (int i = 0; i <= depth; i++) {
		cerr << "od " << setw(3) << static_cast<int>(pinfos[i].ojamadroppeddepthbit) << "/" << setw(3) << static_cast<int>(pinfos[i].eneojamadroppeddepthbit) << spaces;
	}
	cerr << endl;
	for (int i = 0; i <= depth; i++) {
		cerr << "en " << setw(3) << static_cast<int>(pinfos[i].firstenedroppedojamanum) << "/" << setw(1) << static_cast<int>(pinfos[i].enemayojamadroppedturnnum) << "/" << static_cast<int>(pinfos[i].enenotojamadroppedturnnum) << spaces;
	}
	cerr << endl;
	for (int i = 0; i <= depth; i++) {
		cerr << "mos " << setw(2) << static_cast<int>(pinfos[i].enemayojamadropnum) << "/" << setw(3) << static_cast<int>(pinfos[i].enemayskillminusnum) << spaces;
	}
	cerr << endl;

	// ビームサーチの場合の評価
	for (int i = 0; i <= depth; i++) {
		cerr << "bc " << setw(2) << static_cast<int>(pinfos[i].hyouka.chain) << " xr" << setw(2) << static_cast<int>(pinfos[i].hyouka.chainxr) << spaces;
	}
	cerr << endl;
	// その深さで行った行動
	for (int i = 0; i <= depth; i++) {
		if (!isprev || i == depth) {
			cerr << "x " << setw(3) << static_cast<int>(pinfos[i].x) << " r " << setw(2) << static_cast<int>(pinfos[i].r) << spaces;
		}
		else {
			cerr << "x " << setw(3) << static_cast<int>(pinfos[i + 1].px) << " r " << setw(2) << static_cast<int>(pinfos[i + 1].pr) << spaces;
		}
	}
	cerr << endl;

	// その深さで降らすブロックの位置
	for (int by = 1; by >= 0; by--) {
		for (int i = 0; i <= depth; i++) {
			int x, r;
			if (!isprev || i == depth) {
				x = pinfos[i].x;
				r = pinfos[i].r;
			}
			else {
				x = pinfos[i + 1].px;
				r = pinfos[i + 1].pr;
			}
			if (x == 9) {
				x = pinfos[i].hyouka.chainxr / 10;
				r = pinfos[i].hyouka.chainxr % 10;
			}
			if (x >= 0) {
				for (int bx = 0; bx < x; bx++) {
					cerr << " ";
				}
				for (int bx = 0; bx < 2; bx++) {
					cerr << ginfo.blockinfo[ginfo.turn + i][r][bx][by];
				}
				for (int bx = 0; bx < 10 - x - 2; bx++) {
					cerr << " ";
				}
			}
			else {
				cerr << "SKILL     ";
			}
			cerr << spaces;
		}
		cerr << endl;
	}
	// 盤面の表示
	for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
		for (int i = 0; i <= depth; i++) {
			for (int x = 0; x < BOARD_WIDTH; x++) {
				if (field[i][x][y] == 0) {
					// 危険ゾーンでブロックがない場所は ! を表示。
					if (y >= 16) {
						cerr << "!";
					}
					else {
						cerr << ".";
					}
				}
				else if (field[i][x][y] < 10) {
					cerr << field[i][x][y];
				}
				else {
					cerr << "X";
				}
			}
			cerr << spaces;
		}
		cerr << endl;
	}
	cerr << endl;
	// 各列のブロック数（10以上は16進数のようにA、B...で表示）
	for (int i = 0; i <= depth; i++) {
		for (int x = 0; x < BOARD_WIDTH; x++) {
			if (pinfos[i].blocknum[x] < 10) {
				cerr << static_cast<int>(pinfos[i].blocknum[x]);
			}
			else {
				cerr << static_cast<char>('A' + pinfos[i].blocknum[x] - 10);
			}
		}
		cerr << spaces;
	}
	cerr << endl << endl;
	// 最後にdepthの評価値を表示する（この評価値は場合によっては表示内容に意味がないこともある）
	pinfos[depth].hyouka.dump();
	cerr << endl;
}

// 様々な初期化。返り値は所要時間
int init() {
	Timer t;
	// around_bb_table の初期化	
	for (int y = 0; y < BOARD_HEIGHT; y++) {
		for (int x = 0; x < BOARD_WIDTH; x++) {
			around_bb_table[x][y].clear();
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (dx == 0 && dy == 0) {
						continue;
					}
					if (x + dx < 0 || x + dx >= BOARD_WIDTH || y + dy < 0 || y + dy >= BOARD_HEIGHT) {
						continue;
					}
					around_bb_table[x][y].set(x + dx, y + dy);
				}
			}
		}
	}
	// around_bb_x_table の初期化
	for (int i = 0; i < (1 << BOARD_HEIGHT); i++) {
		for (int x = 0; x < BOARD_WIDTH; x++) {
			around_bb_x_table[x][i].clear();
		}
		for (int y = 0; y < BOARD_HEIGHT; y++) {
			if (i & (1 << y)) {
				for (int x = 0; x < BOARD_WIDTH; x++) {
					around_bb_x_table[x][i] |= around_bb_table[x][y];
				}
			}
		}
	}

	// チェインとスキルの点数とお邪魔のテーブルの作成
	chain_scoretable[0] = 0;
	chain_ojamatable[0] = 0;
	for (int i = 1; i <= MAX_CHAIN; i++) {
		chain_scoretable[i] = chain_scoretable[i - 1] + static_cast<int>(floor(pow(1.3, i)));
		chain_ojamatable[i] = static_cast<int>(floor(static_cast<double>(chain_scoretable[i]) / 2.0));
	}
	skill_scoretable[0] = 0;
	skill_ojamatable[0] = 0;
	for (int i = 1; i <= MAX_BLOCK; i++) {
		skill_scoretable[i] = static_cast<int>(floor(25.0 * pow(2.0, static_cast<double>(i) / 12.0)));
		skill_ojamatable[i] = static_cast<int>(floor(static_cast<double>(skill_scoretable[i]) / 2.0));
	}

	// 所要時間を表示
#ifdef DEBUG
	if (parameter.turndebugmsg) {
		cerr << t.time() << " ms" << endl;
	}
#endif
	return t.time();
}

// 探索で見つかった最高評価値
HyoukaData besthyouka;
// ビームサーチで見つかった行動をとった場合の最高評価値
HyoukaData bestbeamhyouka;

// 関数 ai の探索時の、各深さのプレーヤー情報を格納する配列変数
PlayerInfo history[MAX_DEPTH + 1];
// 最高の評価値の各深さの情報を格納する配列変数。思考の可視化のために記録するが、コピーが重いので、
// 最善手のみ記録するバージョンも作る（todo）
PlayerInfo besthistory[MAX_DEPTH + 1];
PlayerInfo bestbeamhistory_log[MAX_DEPTH + 1];

EnemyStatusData enemystatusdata;

// pinfoorig の状態で、pinfoorig.x, pinfoorig.r が示す行動を取った時の結果計算して、pinfo に格納する
// なお、ターン開始時にお邪魔を降らす処理は、この関数を呼ぶ前に実行しておく必要がある点に注意！
// (注：pinfoorig.x == -1, pinfoorig.r == 0 の場合は、スキルの使用行動を表す）
// 不正な行動（x = -1 で r が 0 でない場合か、スキルが発動できない場合）や、ゲームオーバーになる行動の場合は、falseを返す
// 行動の結果 pinfo には下記の情報が記録される
//   skillusedinthisdepth:	この行動でスキルを使用したかどうか
//   skillused:				これまでの行動（今回の行動を含む）でスキルを使用したかどうか
//   skillusedturn:			スキルを使用したまたは10以上したターン（今回スキルを使用した場合のみ更新）
//   prev:					直前のPlayerInfoのポインタ(=&pinfoorig)
//   chain:					この行動によるチェイン数
//   maxchain:					これまでの行動における最大チェイン数
//   getojama:				相手に降らせた累積お邪魔ブロックの総数
//   skillojama:			相手から減らした累積スキルポイントの総数
//	 ojama[1]:				相手の落下予定のお邪魔ブロックの数
bool action(PlayerInfo& pinfoorig, PlayerInfo& pinfo) {
	// 消滅するブロックの位置を表すビットボード
	BitBoard deletebb;
	// pinfoorigの行動を x, r にコピーする
	int x = pinfoorig.x;
	int r = pinfoorig.r;

	// インデックスが表す番号のブロックが動いたか（次に消滅判定が必要かどうか）、消滅したか（周囲のブロックの範囲の再計算が必要）どうかを表す。初期化処理を省くため、
	// blockmovecount と等しい場合に動いたことを表す。その場合、
	// ブロックの消滅チェックに使うので、そのブロックの対（10-そのブロックの番号）のブロックも同時に動いたことにする
	// blockmovecount + 1 と等しい場合消滅したことを表す。
	// blockmoved の値が用済みになった時点で blockmovecount に 2 を加算する
	// なお、blockmovecountは連鎖のたびに2ずつ加算されるが、最大連鎖数は約80なので、blockmoved の型は uint8_tで足りる
	uint8_t blockmoved[11];
	// 初期状態として、 blockmovecount は 1、blockmoved はすべて 0 にしておく
	uint8_t blockmovecount = 1;
	memset(blockmoved, 0, 11);

	// スキル使用行動の場合
	if (x == -1) {
		// r == 0で、スキルポイントが 80 以上あり、5のブロックが存在する場合のみスキルを使用できる
		// そうでなければ不正な行動なので false を返す
		if (r > 0 || pinfoorig.skill[0] < 80 || pinfoorig.bb[5].pcount() == 0) {
			return false;
		}
	}
	// pinfoorig に対して x, r の行動を行った結果を保存する pinfo に pinfoorig コピーする
	pinfo = pinfoorig;
	// ただし、スキルはまだ使用していないので false を代入する
	pinfo.skillusedinthisdepth = false;
	// 直前の PlayerInfo のアドレスを設定する
	pinfo.prev = &pinfoorig;

	// スキル以外の行動の場合（スキル使用の場合は、パックは落ちてこないのでこの処理は行わない）
	if (x != -1) {
		// パック情報を盤に置く
		// パックの情報を表す 2x2 の配列のループ
		for (int px = 0; px < 2; px++) {
			// パックを落下する列のブロック数を表す変数への参照
			uint8_t& blocknum = pinfo.blocknum[x + px];
			// パック内の下のブロックから落とす必要があるので、py は 0, 1 の順で処理する
			for (int py = 0; py < 2; py++) {
				// 落下するブロックの種類
				int block = ginfo.blockinfo[pinfo.turn][r][px][py];
				// block が 0 の場合は、ブロックが存在しないので、0より大きい場合のみ落とす
				if (block > 0) {
					// 任意のブロックの Bitboard である pinfo.bb[0] の x + px 列にブロックを追加(追加する場所は(x + px, blocknum))
					pinfo.setbb(0, x + px, blocknum);
					// block のブロックのみの BitBoard である pinfo.bb[block] の x + px 列にブロックを追加
					pinfo.setbb(block, x + px, blocknum);
					// block のブロックの周囲8マスの位置を表す BitBoard にブロックが落下した (x + px, blocknum) の
					// 周囲8マスのブロックを追加する（あらかじめ init で計算済の、任意の座標の周囲8マスの位置を表す around_bb_table と OR を取れば良い）
					pinfo.bb[block + 10] |= around_bb_table[x + px][blocknum];
					// block のブロックが移動したことを記録する（ブロックの消滅判定を、移動したブロックとその対のブロックのみ行うことによる処理の高速化のため）
					blockmoved[block] = blockmovecount;
					// そのブロックの対のブロックも移動したことにする
					// （5のブロックの場合は、上記と同じ代入を行うが、条件分岐させてこの代入を行わない処理を行っても高速化は望めないのでそのまま実行する）
					blockmoved[10 - block] = blockmovecount;
					// x + px 列のブロックを1増やす
					blocknum++;
				}
			}
		}
	}

	// ブロック消去処理開始
	// チェイン数を表す変数への参照。初期化もしておく
	uint8_t& chain = pinfo.chain = 0;
	// ブロックが消去されたかどうかを表すフラグの定義
	bool deleted;
	// ブロックが消去されなくなるまで繰り返す。ただし、必ず1回は実行する
	do {
#ifdef DEBUG
		// チェインを視覚化するパラメータをONにした場合のデバッグ表示
		if (parameter.showchain) {
			cerr << "chain " << static_cast<int>(chain) << endl;
			pinfo.dump_field();
		}
#endif
		// このループでブロックが消去されていないことにする
		deleted = false;
		// スキルを使用した場合で、まだスキルの消去処理を行っていない場合の処理
		if (x == -1 && !pinfo.skillusedinthisdepth) {
			// スキルポイントを 0 にする
			pinfo.skill[0] = 0;
			// 消去されたブロックを計算する。
			// A: 5のブロック(pinfo.bb[5])と、5の周囲のブロック(pinfo.bb[15])の和集合が消去される可能性のあるブロック
			// B: お邪魔ブロックでないブロックがある場所が消去可能なブロック
			// A AND B が消去されるブロックとなる
			deletebb = (pinfo.bb[5] | pinfo.bb[15]) & BitBoard::andnot(pinfo.bb[10], pinfo.bb[0]);
			// 消去されるブロックの数を計算する
			int deletenum = deletebb.pcount();
			// 相手に降らせるお邪魔ブロックの数を計算し、累積に加算する
			pinfo.getojama += skill_ojamatable[deletenum];
			// 相手のお邪魔ブロック数を増やす
			pinfo.ojama[1] += skill_ojamatable[deletenum];
			pinfo.getskillojama = skill_ojamatable[deletenum];
			// スキルによるブロック消去はチェインに数えない。ループ時に chain を1増やすので、-1 にしておく
			chain = -1;
			// スキルを使用したことを変数に記録する
			pinfo.skillusedinthisdepth = true;
			pinfo.skillused = true;
			pinfo.skillusedturn = pinfo.turn;
		}
		// ブロック落下によるブロック消滅判定と消滅処理
		else {
			// 消去されたブロックを表す BitBoard のクリア
			deletebb.clear();
			// 各ブロックについて、消滅した場所を deletebb に記録する
			for (int i = 1; i <= 9; i++) {
				// 動いたブロックとその対になるブロックだけチェックすればよい。それは blockmoved を調べればわかるようにしてある
				if (blockmoved[i] == blockmovecount) {
					// i 番のブロックと、10 - i 番目のブロックの周囲8マスのブロックの BitBoard の積集合が、消滅するブロック。それを deletebb に加える
					deletebb |= pinfo.bb[i] & pinfo.bb[20 - i];
				}
			}
		}
		// 今回のブロック消滅判定は終了したので、blockmoved の値をすべてクリアするため、blockmovecount を 2 増やす
		blockmovecount += 2;

		// 消滅したブロックがある場合の処理
		if (!deletebb.iszero()) {
			// このループでブロックが消滅したことを表す変数のフラグを立てる
			deleted = true;
			// チェイン数を 1 増やす
			chain++;
			// 各列についてブロックを削除する
			for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
				// x2 列目に消滅するブロックが存在する場合
				uint32_t blocks = deletebb.getx(x2);
				if (blocks != 0) {
					// x2 列目のブロックが存在しない場所を表すマスクビットを計算する(NOTを取る）
					uint32_t mask = ~blocks;
					// 任意のブロック（0）と各ブロック(1～9)とお邪魔ブロック(10)を表すビット列をすべて消えたブロックの分だけずらす
					for (int i = 0; i <= 10; i++) {
						// x2 列にブロックが存在する場合だけずらせば良い
						uint32_t bbx = pinfo.getbbx(i, x2);
						if (bbx != 0) {
							// BMI命令の、pext_u32 を使えば一発でずらせる
							uint32_t newbbx = _pext_u32(bbx, mask);
							// ずらす前とずらした後が異なる場合のみ処理を行えばよい
							if (bbx != newbbx) {
								// 新しい値をセットする
								pinfo.setbbx(i, x2, newbbx);
								//// 任意のブロックの場合は、その列のブロック数を計算しなおす
								if (i == 0) {
									pinfo.blocknum[x2] = popcnt(newbbx);
								}
								// 消滅後の x2 列に 1～9 のブロックが残っていた場合は、連鎖の対象となるので、
								// そのブロックと対になるブロックが動いたことを記録する
								// (記録しても利用されないお邪魔ブロックを表す 10 と 任意のブロックの 0 のブロックの blockmoved も
								//  記録されてしまうが、特に害はなく、条件分岐で記録を排除しても高速化はみこめなさそうなのでそのままにしておく）
								// ここで記録した blockmoved はこの後の各ブロックの周囲9マスのブロックの再計算と、次のループでのブロック消滅判定で使用する
								else if (newbbx != 0) {
									blockmoved[i] = blockmovecount;
									blockmoved[10 - i] = blockmovecount;
								}
								// ブロックが完全に削除されていて（newbbx が 0）、連鎖の対象となっていない場合でも、そのブロックの周囲8マスの再計算は必要。
								// そのことを表すため、blockmoved[i] に消滅したことを表す blockmovecount + 1 を代入する
								else if (blockmoved[i] != blockmovecount) {
									blockmoved[i] = blockmovecount + 1;
								}
							}
						}
					}
				}
			}
			// 各ブロック（1～9）の周囲8マスのブロックの再計算
			// 消滅したブロックの周囲の8マスのビットをOFFにするという方法では、OFFにしたビットの周囲に消滅したブロックと同じ
			// 番号のブロックが存在した場合はうまくいかないので、一から計算しなおすがある
			for (int i = 1; i <= 9; i++) {
				// 移動、削除されたブロックのみ再計算すればよい
				if (blockmoved[i] >= blockmovecount) {
					// 周囲8マスのブロックを表すBitBoardをクリアする
					pinfo.bb[i + 10].clear();
					// 各列について計算する
					for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
						// x2 列の i 番のブロックのビットパターンと around_bb_x_table を使ってまとめてその列のすべてのブロックに対する
						// 周囲8マスのブロックのBitBoardを取得して OR をとれば良い
						pinfo.bb[i + 10] |= around_bb_x_table[x2][pinfo.getbbx(i, x2)];
					}
				}
			}
		}
		// 消滅するブロックが存在しなくなるまで繰り返す
	} while (deleted);

	// 10連鎖以上を組んだ場合、skillusedturn に入れておく
	if (pinfo.chain >= 10) {
		pinfo.skillusedturn = pinfo.turn;
	}

	if (pinfo.chain > pinfo.maxchain) {
		pinfo.maxchain = pinfo.chain;
	}
	// ゲームオーバー判定
	// お邪魔ブロックが降ってきていない場合
	if (!pinfo.ojamadropped) {
		// スキルを使っていない場合は、落下させたブロックの2列のみ判定すれば良い
		if (!pinfo.skillusedinthisdepth) {
			if (pinfo.blocknum[x] >= GAMEOVER_Y || pinfo.blocknum[x + 1] >= GAMEOVER_Y) {
				// ゲームオーバーになるので、飛ばして次へいく
				// 結果としてすべての行動がゲームオーバーになる場合は、評価値が一度も計算されず、besthyouka の total が -INF のままとなるので判定できる
				return false;
			}
		}
		// お邪魔ブロックが降らず、スキルを使った場合は絶対にゲームオーバーにならない
	}
	// お邪魔ブロックが降ってきた場合
	else {
		// 各列についてチェックする
		for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
			// 一つでもゲームオーバーとなる高さに達していればゲームオーバーとする
			if (pinfo.blocknum[x2] >= GAMEOVER_Y) {
				return false;
			}
		}
	}

	// 相手に降らせるお邪魔ブロックの数を計算し、累積に加算する
	pinfo.getojama += chain_ojamatable[chain];
	// 相手のお邪魔ブロック数を増やす
	pinfo.ojama[1] += chain_ojamatable[chain];

	// チェインがあった場合自分のスキルを8増やす
	// 3チェイン以上の場合の、相手のお邪魔を減らす処理はここでは行わない
	// (先に相手のスキルを増やす処理（増やした結果100を超えた場合は100に補正する）を行ってから減らさないとうまくいかないので）
	if (chain > 0) {
		// 自分のスキルを 8 増やす
		pinfo.skill[0] += 8;
		// 100を超えたらここで100に補正する
		if (pinfo.skill[0] > 100) {
			pinfo.skill[0] = 100;
		}
	}

	// turnを1増やす
	pinfo.turn++;
	return true;
}

// 通常時の評価値を計算する
void PlayerInfo::calchyouka() {
	// 自分のフィールド内のお邪魔の数を計算する
	int onum = bb[10].pcount();

	// お互いの落下予定のお邪魔ブロック数による評価値
	double hyouka_ojama;
	// 基本は「敵のお邪魔の数」 - 「自分のお邪魔の数」を使うが、
	// お邪魔ブロックの1の位の価値は10の位の価値よりも低い（実際に落ちてこないため）ので、
	// 1の位の評価値を 0.2 倍して計算する
	hyouka_ojama = ((ojama[1] - ojama[0]) / 10) * 10 + ((ojama[1] - ojama[0]) % 10) * 0.2;

	// 上記で計算した基本値から下記の要素を追加する
	// 20ターン以内で、敵が落としてくる可能性のあるお邪魔の数が20未満（9連鎖以下）の場合は、
	// そのような不利な行動をしてくることはまずないと仮定して評価値に50を加える
	if (turn <= 20 && enemayojamadropnum != 0 && enemayojamadropnum < 20) {
		hyouka_ojama += 50;
	}
	// 自分のフィールドに落下済のお邪魔が70以下で、敵が落としてくる可能性のあるお邪魔の数が 15 未満の場合は、
	// そのような行動を中盤にしてくることはまずないと仮定して敵の落としてくる可能性のあるお邪魔の数によるマイナス評価を 10% にするように評価値を加算する
	else if (onum <= 70 && enemayojamadropnum != 0 && enemayojamadropnum <= 15) {
		hyouka_ojama += enemayojamadropnum * 0.9;
	}
	// 自分が大連鎖をしない場合（相手に降らすお邪魔の合計が10未満の場合。これ以上降らせてしまうと、この後しばらくの間、大連鎖できる見込みがなくなるため除外する）に
	// ginfo.hyoukabonusx, ginfohyoukabonusr の行動をとった場合、
	// 敵がこちらにお邪魔を降らしてきた深さのボーナスが getojama より多い場合は、この後 ginfo.hyoukabonus[enemayojamadroppeddepth] だけ相手にお邪魔を降らす見込みがあるので
	// その分評価値を加算する
	if (getojama < 10 && enemayojamadroppeddepth > 0 && ginfo.hyoukabonusx == fx && ginfo.hyoukabonusr == fr && ginfo.hyoukabonus[enemayojamadroppeddepth] > getojama) {
		hyouka_ojama += ginfo.hyoukabonus[enemayojamadroppeddepth] - getojama;
	}
	// 相手に降らすお邪魔が15以下の時は、あまり効果がないとして、その評価を 10% に減らす
	if (getojama <= 15) {
		hyouka_ojama -= getojama * 0.9;
	}

	// 落下済のお邪魔の数の差による評価
	// 敵のフィールドのお邪魔の最小値から自身のフィールドのおじゃまの数を引いたものを使う
	int16_t hyouka_droppedojama = minojamanum - bb[10].pcount();
	
	// 敵がお邪魔を落とすかどうか選択可能な場合や、降らすことができるのに降らさないという行動をとった時の評価値
	// 敵がお邪魔を落とすか落とさないかを選択できたほうが状況が不確定なので負の評価を加算する
	// 敵がお邪魔を落とすことができるのに落とさなかった回数が多いほど、そのような状況にはなりにくいと考えて評価値を加算する（あまり大きな値にしないほうがよさそう）、
	double hyouka_enemaydrop = enemayojamadroppedturnnum * parameter.enemaydrophyouka + enenotojamadroppedturnnum * parameter.enenotdrophyouka;

	// 致死量かどうかのチェック
	// こちらの攻撃により、この深さ以降で敵は連鎖できないものと仮定する
	// （カウンター戦術の普及により、この仮定はもはや通用しない？？todo）
	// 従って、考慮に入れるのは敵のスキルによるお邪魔のみ
	// この時点で敵に致死量のお邪魔が降る場合のみ計算する
	double hyouka_enedead = 0;
	// 以下の場合、相手を殺しに行くための評価値を計算する
	// 相手にお邪魔を降らす場合で、(todo: 最初にお邪魔を降らせているのは必須？）
	// 致死量から2段目まで以上を降らせることが可能な場合(todo: 2段目までで良い？）
	if (ojama[1] >= 10 && ojama[1] >= deadojamanum[1] - 30) {
		// 相手がスキルを発動可能になる最短のターン数を計算する
		int eneskillturn;
		if (skill[1] >= 80) {
			eneskillturn = 0;
		}
		else {
			eneskillturn = (87 - skill[1]) >> 3;
		}
		// この深さに到達している時点で相手にお邪魔が降ることによって、大連鎖ができなくなっていると仮定する
		// （この仮定が外れた場合は負ける可能性大）
		// 敵がスキル発動可能なターンが、死亡ターンより後の場合は100%死亡させられる
		if (eneskillturn * 10 > deadojamanum[1]) {
			// なるべく早いターンで殺したいので、スキル発動ターンが速いほうを優先する
			// skillusedturn は最大500なので、負の値にならないように注意！
			hyouka_enedead = 1e10 - skillusedturn * 1e7;
		}
		// そうでない場合
		else {
			// 死亡ターンの計算
			int deadturn;
			// まず、敵がこれまでスキルを使ったかどうかによって分けて考える
			// 敵がスキルを使ったと仮定した場合は、この後スキルを発動することは現実的にほぼ不可能。
			// できたとしても、せいぜい20個程度しかお邪魔を降らすことはできないと考える
			// ここに来ている時点で敵に致死量-2段目までのお邪魔を降らせており、スキルによって相殺されても致死量-4段目となり、
			// 十分な致死量とみなせる。従って、この後敵がスキルを使った場合に、敵が状況を回避できなければ殺しに行って良い

			// 敵がスキルを使っていない場合について考える。
			// その場合のお邪魔の数の計算(差異の分だけ増やす）
			//  ojama[1]>=10の条件でここに来ているので、ojama[0] >= 0のケースはあり得ないので、単純に足せばよい
			int ojamanum = ojama[1] + eneskillnotuseojamadiff;
			// 敵が最大効率で、スキルを使わずに放置すると死亡するターンぎりぎりにスキルを使ってきたときのことを考える
			// なお、こちらはお邪魔を大量に降らせていると仮定して、敵は大連鎖を行えなくなっていると仮定する
			// まず、敵はスキルを80以上溜める必要がある。そのためには最低でも eneskillturn * 2 個のブロックを消去する必要がある
			// その分をブロック数と、スキルで消去可能なブロック数から引いておく
			// （最大効率を目指すため、敵は連鎖を行わない、3つ以上のブロックを消さないものと仮定する）
			int blocknum = eneskillnotuseminblocknum - eneskillturn * 2;
			int skillblocknum = eneskilldeleteblocknum - eneskillturn * 2;
			// 次に、敵が死亡するターンの計算を行う。
			// ほぼ致死量のお邪魔を降らせているはずなので、毎ターン10個のお邪魔ブロックが降ってくるものとする
			// また、敵はブロックを消さないと仮定し、毎ターン3つのブロックが積まれていくとして計算する
			// 死亡ターン数はそうして計算したターン数と、deadojamanum[1] / 10 のうち小さいほう
			deadturn = min((160 - blocknum) / 13 + 1, deadojamanum[1] / 10);
			// 相手がスキルで消せるブロックの数は毎ターン1増えるものとする todo: 1でOK?
			// （こちらが大量にお邪魔を降らせているはずなので、場合、スキルで消せるブロックを一気に増やすことは困難だという仮定）
			skillblocknum += deadturn;
			if (ojamanum - skill_ojamatable[skillblocknum] + deadturn * 10 + blocknum - skillblocknum >= deadojamanum[1] - 20) {
				// なるべく早いターンで殺したいので、スキル発動ターンが速いほうを優先する
				// skillusedturn は最大500なので、負の値にならないように注意！
				hyouka_enedead = 1e9 - skillusedturn * 1e6;
			}
		}
	}

	// 通常のブロック数（多いほうが連鎖しやすいので、評価値にはこの値の 0.01 倍を加算する
	uint8_t hyouka_normalblocknum = bb[0].pcount() - onum;
	// スキルによって消去可能なブロックの数
	uint16_t hyouka_skilldeleteblocknum = calcskilldeleteblocknum();
	int mincheckonum;
	double mulskillojama;
	// スキル発動を目指している場合
	if (ginfo.skillmode) {
		mincheckonum = 0;
		mulskillojama = 1.0;
	}
	// 連鎖を目指している場合
	else {
		// お邪魔の数が50未満の時は、スキルは狙わない
		// スキルによる評価値を半分にする
		mincheckonum = 50;
		mulskillojama = 0.5;
	}

	double hyouka_skillojama;
	// 落下隅のお邪魔の数が mincheckonum 以上の時のみスキルによる評価値を計算する（早めにスキルを目指さないようにするため） 
	if (onum >= mincheckonum) {
		// スキルを発動不可能な場合
		if (skill[0] < 80) {
			// skillojama に 消去可能なブロック数によって相手に降らせるお邪魔の数 * (スキルポイント + 1) / 81 を設定
			// 1 を足しているのはスキルポイントが0の場合でも正の評価値を与えたいため
			hyouka_skillojama = skill_ojamatable[hyouka_skilldeleteblocknum] * (skill[0] + 1.0) / 81.0 * mulskillojama;
		}
		// スキルを発動可能な場合は、スキルを発動した場合に降ってくるお邪魔の数を skillojama に設定する
		else {
			hyouka_skillojama = skill_ojamatable[hyouka_skilldeleteblocknum];
		}
	}
	else {
		hyouka_skillojama = 0;
	}
	// 敵のスキルに関する評価値
	// ただし、あまり評価しすぎても仕方がないので、あと3回チェインした場合に使用できるようになる
	// スキルポイントが 56 以上の場合のみ評価する
	double hyouka_eneskillojama;
	// parameter.calceneskill が false の時は評価しない
	if (!parameter.calceneskill || skill[1] < 56) {
		hyouka_eneskillojama = 0;
	}
	// スキルポイント/80 割合のさらに1/5で評価する
	// 1/2だと、この深さ落ちていないにも関わらず、この深さより前にスキルでお邪魔を落とされた場合よりも評価値が悪くなるケースがあった
	// そもそもここは計算しないほうがいいかも？
	else if (skill[1] < 80) {
		hyouka_eneskillojama = skill_ojamatable[eneskilldeleteblocknum] * skill[1] / 80.0 / 5.0;
		// スキルを使った場合の評価値と二重計算にならないように、相手に送るお邪魔の差異の分だけ引く
		hyouka_eneskillojama -= eneskillnotuseojamadiff;
		//hyouka.normalblocknum = eneskillnotuseojamadiff;
		if (hyouka_eneskillojama < 0) {
			hyouka_eneskillojama = 0;
		}
	}
	else {
		// こちらは確実に次で降ってくるのでそのままの値で計算する
		hyouka_eneskillojama = skill_ojamatable[eneskilldeleteblocknum];
		// 既にスキルを使った場合の評価値と二重計算にならないように、相手に送るお邪魔の差異の分だけ引く
		hyouka_eneskillojama -= eneskillnotuseojamadiff;
		if (hyouka_eneskillojama < 0) {
			hyouka_eneskillojama = 0;
		}
	}

	// ブロックが致死量に迫っている場合の評価値
	int16_t hyouka_limit = 0;
	// スキルが使えない場合のみ計算する
	if (skill[0] < 80) {
		for (int i = 0; i < BOARD_WIDTH; i++) {
			// あまりよくなさそうなので limit1 と limit2 は廃止
			// ブロックの数が 15 以上の列一つに加算する（hyouka_limit_x は負の値にしておく）
			//if (blocknum[i] == GAMEOVER_Y - 2) {
			//	hyouka.limit += parameter.hyouka_limit_2;
			//}
			//else if (blocknum[i] == GAMEOVER_Y - 1) {
			//	hyouka.limit += parameter.hyouka_limit_1;
			//}
			//// スキルが使えない場合で、致死量の場合は一つにつき -200 する
			//else 
			if (blocknum[i] >= GAMEOVER_Y) {
				hyouka_limit += parameter.hyouka_limit;
			}
		}
	}
	// 連鎖数による評価
	double chainhyouka = 0;
	// 4連鎖以上した場合
	if (maxchain > 3) {
		// 敵のフィールドに落下済のお邪魔が存在しない場合で、 parameter.firstminchainnum 連鎖以下の場合は、効果がないとして、parameter.firstminchainminus の分だけ評価値を足す（負の値）
		if (firstenedroppedojamanum <= 0 && maxchain <= parameter.firstminchainnum) {
			chainhyouka = parameter.firstminchainminus;
		}
		// 敵のフィールドに落下済のお邪魔が20以下の場合で、 parameter.secondminchainnum 連鎖以下の場合は、効果がないとして、parameter.secondminchainminus の分だけ評価値を足す（負の値）
		else if (firstenedroppedojamanum <= 20 && maxchain <= parameter.secondminchainnum) {
			chainhyouka = parameter.secondminchainminus;
		}
	}
	double penalty = 0;
	// こちらが最初に連鎖した場合に相手にカウンターで大連鎖される場合の評価値のペナルティを計算する
	if (fchain > ginfo.maxfirstchain && enemayojamadropnum < ginfo.firstchainpenalty) {
		// 既にこの時点で落とされているお邪魔から、将来落とされるお邪魔の数を引いたものをペナルティとする
		penalty = enemayojamadropnum - ginfo.firstchainpenalty;
	}
	double bonus = 0;
	// こちらが最初に連鎖した場合に、相手がカウンターできない場合の評価値のボーナスを計算する
	if (fx == ginfo.firstx && fr == ginfo.firstr) {
		bonus = ginfo.firstbonus;
	}

	// 敵は死亡していないのでこの評価値を 0 とする
#ifdef DEBUG
	hyouka.enedeaddepth = 0;
#endif
	// 最終評価値の計算
#ifdef DEBUG
	hyouka.ojama = hyouka_ojama;
	hyouka.skillojama = hyouka_skillojama;
	hyouka.eneskillojama = hyouka_eneskillojama;
	hyouka.enedead = hyouka_enedead;
	hyouka.enemaydrop = hyouka_enemaydrop;
	hyouka.limit = hyouka_limit;
	hyouka.droppedojama = hyouka_droppedojama;
	hyouka.normalblocknum = hyouka_normalblocknum;
#endif
	hyouka.total = hyouka_ojama + hyouka_droppedojama + hyouka_skillojama + hyouka_limit + hyouka_normalblocknum * 0.01 + hyouka_enedead - hyouka_eneskillojama + hyouka_enemaydrop + chainhyouka + penalty + bonus;
}

// ビームサーチの場合の評価値の計算
void PlayerInfo::calcbeamhyouka() {
	PlayerInfo pinfo;
	// 見つかった評価値の最大値
	double maxhyouka = -100000;
	// 最大チェーン数
	uint8_t mchain = 0;
	// 最大チェーン時の行動を x, r とした際の x * 10 + r
	int maxxr;
	for (x = 0; x <= 8; x++) {
		for (r = 0; r < 4; r++) {
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
			if (!action(*this, pinfo)) {
				continue;
			}
			// チェーン数
			uint8_t cnum = pinfo.chain;
			// 行動後の通常のブロック数（多いほうが連鎖後の状況が良いという仮定）
			int blnum = pinfo.bb[0].pcount() - pinfo.bb[10].pcount();
			// 評価値。チェーンで落とせるブロックの数 * 100 + チェーン後のブロック数
			double h;
			h = chain_ojamatable[cnum] * 100 + blnum;
			// 最初（1ターン目の大連鎖を組むためのビーム探索時）のみ上3つ分に積まないように制限する（積みすぎるとカウンターしずらくなるため）
			if (ginfo.firstbeamcheck) {
				for (int i = 0; i < BOARD_WIDTH; i++) {
					if (blocknum[i] >= GAMEOVER_Y - 3) {
						h = 0;
						break;
					}
				}
			}
			// 最大の評価値の記録
			if (maxhyouka < h) {
				maxhyouka = h;
				mchain = cnum;
				maxxr = x * 10 + r;
			}
		}
	}
	// 評価値の maxchain (ビーム探索時に発見した最大連鎖数） の更新
	if (this->maxchain < mchain) {
		this->maxchain = mchain;
	}

	// 今回みつかった最大チェーン数とそのチェーンを実現する行動の記録
	hyouka.chain = mchain;
	hyouka.chainxr = maxxr;
	// maxchain の記録
	if (hyouka.maxchain < mchain) {
		hyouka.maxchain = mchain;
	}
	// maxhyoukaが負の場合は死んでいる
	if (maxhyouka < 0) {
		hyouka.total = 0;
	}
	// そうでなければ、評価値は過去に見つかった最大チェーン数によるお邪魔落下数 * 1000 + 今回の最大評価値とする
	else {
		hyouka.total = chain_ojamatable[hyouka.maxchain] * 1000 + maxhyouka;
	}
}

// ai（味方の探索）。ginfo.searchDepth までの深さの局面の全探索。深さ優先探索
// 返り値として、parameter.ai2 が true の場合はそのノードの評価値を返す。falseの場合は0を返す
// 各深さのデータは引数では渡さずに、history の配列に格納する
// 味方と敵の区別は ginfo.isenemy で区別する（引数にしたほうが良いか？）
// bhistory はこれまでに見つかった最高の評価値に至る行動を表す PlayerInfo の配列
// bbeamhistory はビーム評価の場合
// ginfo.searchDepth で評価関数を計算する
// 敵の場合は、味方の探索深さ-1まで探索を行う。また、各ノードでこちらに影響する条件の最大値である
// 「お邪魔を降らせた数」、「相手のスキルを減らした数」を記録する（評価関数ではない）
// estatus は、味方の探索時に、敵の行動の結果、味方のお邪魔を増やしたり、スキルを減らしたりする処理で使用する
//// 引数 depth は探索する深さ
double mysearch(int depth, PlayerInfo *history, PlayerInfo *bhistory, PlayerInfo *bbeamhistory, HyoukaData& bhyouka, HyoukaData& bbeamhyouka, EnemyStatusData& estatusdata) {
	// この深さのノードのプレーヤーの情報への参照
	PlayerInfo& pinfoorig = history[depth];
	// pinfoorig に対してプレーヤーが行動を行った結果の局面を表すプレーヤーの情報への参照
	PlayerInfo& pinfo = history[depth + 1];

#ifdef DEBUG
	// この関数を呼んだ回数のカウント（デバッグ用）
	ginfo.aicount++;
#endif
	// お邪魔落下処理
	// 深さが 0 または、深さが ginfo.ai2deth 以外の場合に落下処理を行う
	// ginfo.ai2depth が 1 以上の場合は、その深さにおける最善手を計算している（デバッグ用）
	// その場合は、お邪魔は落下済の状態でここにくるので、落下処理を行う必要はない
	if (depth == 0 || depth != ginfo.ai2depth) {
		pinfoorig.ojamadrop(depth);
	}

	// リーフノードまたは、最終ターンを過ぎた場合の処理
	if (depth == ginfo.searchDepth || pinfoorig.turn == MAX_TURN) {
		// 評価値を計算する関数を呼ぶ
		pinfoorig.calchyouka();
		// parameter.ai2 の場合は評価値を返す
		if (parameter.ai2) {
			return pinfoorig.hyouka.total;
		}
		// そうでない場合は、これまでに見つかっている最高の評価と比較し、より評価値が高い場合は bhyouka と bhistory を更新する
		HyoukaData& hyouka = pinfoorig.hyouka;
		if (hyouka.total > bhyouka.total) {
			bhyouka = hyouka;
			memcpy(bhistory, history, sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
		}
		// 初手がビーム探索で見つかった行動をとっていた場合は、同様に最高の評価を更新する
		if (history[0].x == ginfo.beamx && history[0].r == ginfo.beamr && hyouka.total > bbeamhyouka.total) {
			bbeamhyouka = hyouka;
#ifdef DEBUG
			// こちらの記録はデバッグ時のみ必要
			if (parameter.debugmsg) {
				memcpy(bbeamhistory, history, sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
			}
#endif
		}
		// 終了する
		return 0;
	}

	// 置換表を使って同じ局面が出てきた場合の処理を行う
	TT_DATA *tdata = nullptr;
	Key key;
	// 深さが 0 の場合は置換表は使わない（スレッド版で置換表を使ってしまうと、ビーム探索の行動が排除されてしまう可能性があるため）
	if (parameter.usett && depth != 0) {
		// ハッシュ地を計算
		key = pinfoorig.calc_hash();
		bool found;
		// 置換表を引く
		tdata = tt.findcache(key, found);
		// 存在した場合は登録されている評価値を返す
		if (found) {
			return tdata->get();
		}
	}

	// この深さでの行動を表す変数への参照（x:落下位置（-1の場合はスキルを使う),r:回転数）
	int8_t& x = pinfoorig.x;
	int8_t& r = pinfoorig.r;
	// 最大評価値の初期化
	double maxhyouka = -INF;
	// 可能な行動をすべて試す
	for (x = -1; x <= 8; x++) {
		// 深さ 0 の場合でスレッドを使う場合は、xはpinfoorig.px に固定して実行する（他のxは他のスレッドで実行するのでここでは計算しない）
		if (depth == 0 && parameter.usethread) {
			x = pinfoorig.px;
		}
		for (r = 0; r < 4; r++) {
			// 深さ0における行動を fx, fr に記録する
			if (depth == 0) {
				pinfoorig.fx = x;
				pinfoorig.fr = r;
			}
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
			if (!action(pinfoorig, pinfo)) {
				// 不正な行動、ゲームオーバー時は飛ばす
				continue;
			}
			// 深さ0におけるチェーン数を fchain に記録する
			if (depth == 0) {
				pinfo.fchain = pinfo.chain;
			}

			// etatus の情報を使って、敵のこの深さでの行動の結果を反映させる。
			// この時点では、まだこのターンの開始時の敵のスキルポイントの情報を使用したいので、
			// 味方のチェーンによる敵のスキルポイントを減らす処理を行う前にこの処理を行う必要がある
			// 敵のスキルの使用状況にる estatus のインデックス番号を表す変数。初期値としてスキルを全く使用しないことを表す 0 を設定しておく
			int skillindex = 0;
			// もう一つの　playerinfo をチェックするかどうか
			bool checkanotherpinfo = false;
			// もう一つの playerinfo の情報を格納する変数
			PlayerInfo anotherpinfo;
			// 深さが、敵の探索深さ未満の場合のみ計算する
			if (depth < ginfo.enesearchDepth) {
				// この深さで敵がスキルを使えるかどうかのチェック
				if (pinfo.skill[1] >= 80 && pinfo.eneskilldeleteblocknum > 0) {
					// 過去に敵がスキルを使用可能だったとしても、その時点で敵がスキルを使用していたかどうかが不明なので
					// estatus のすべての可能性の中のいいとこどりをしたインデックス(4)を使用する
					// なお、ここで敵がスキルを使用するかどうかは不確定なので、敵のスキルポイントを0にする処理は行わない
					skillindex = 4;
					// 使用した可能性があることを表すフラグを立てる
					pinfo.mayeneuseskill = true;
				}
				// 使えない場合は、過去に使った可能性があるかどうかで分ける
				// 使った可能性があれば過去に使用した可能性がある場合のインデックス番号1を採用する
				// なお、pinfo.mayeneuseskill は実際にはスキルを使用不能な場合も含まれる可能性がある
				// (敵のスキルポイントは estatus の chain が 1 以上の時に換算するが、実際にはスキルが増えないケースがあるため）
				// estatusdata.data[1][pinfo.eneojamadroppeddepthbit][depth + 1].isdead が true の場合は次で死亡するので除外する
				else if (pinfo.mayeneuseskill && !estatusdata.data[1][pinfo.eneojamadroppeddepthbit][depth + 1].isdead) {
					skillindex = 1;
				}

				// 敵が死亡している場合はこれ以上探索を続ける必要はない
				if (estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].isdead) {
					// 敵が死亡した場合の評価を計算（次の深さで敵は死亡するので、引数には depth + 1 を入れる）
					pinfoorig.calcenedeadhyouka(depth + 1);
					HyoukaData& hyouka = pinfoorig.hyouka;
					// ai2がfalseの場合は、ここがリーフノードなので、ここで最高評価値の情報の更新を行う
					if (!parameter.ai2) {
						// 評価値の最高値の更新
						if (hyouka.total > bhyouka.total) {
							bhyouka = hyouka;
							memcpy(bhistory, history, sizeof(PlayerInfo) * (depth + 1));
						}
						// ビーム探索で得られた行動に対する最高評価値の情報の更新
						if (history[0].x == ginfo.beamx && history[0].r == ginfo.beamr && hyouka.total > bbeamhyouka.total) {
							bbeamhyouka = hyouka;
#ifdef DEBUG
							if (parameter.debugmsg) {
								memcpy(bbeamhistory, history, sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
							}
#endif
						}
					}
					// ai2 が true の場合は深さが0の場合に、最高評価値の情報の更新を行う
					else {
						if (depth == 0) {
							bhyouka.total = hyouka.total;
							bhistory[0] = history[0];
#ifdef DEBUG
							if (parameter.debugmsg) {
								bhistory[1] = pinfo;
							}
#endif
						}
					}
					// 深さ0以外で、置換表を使う場合は、置換表に得られた評価値をセットする
					if (parameter.usett && depth != 0) {
						tdata->set(key, pinfoorig.hyouka.total);
					}
					// ginfo.ai2depth（探索開始時の深さ）と等しい場合は、その深さにおける最善手を調べているので、bhistory にその深さの最善手を記録する（デバッグ用）
#ifdef DEBUG
					if (parameter.debugmsg && parameter.ai2 && depth == ginfo.ai2depth) {
						bhistory[depth + 1] = pinfo;
					}
#endif
					// それ以上探索する必要はないので評価値を返して終了
					return pinfoorig.hyouka.total;
				}

				// estatusdata.data の「敵がスキルを使用できるかどうか」、「これまでにお邪魔が降ってきたターン数を表すビット」、「次の深さ」の情報を
				// 使って、味方にお邪魔を降らす
				// todo: 自分にとって「このターンにおける」最悪の行動を相手がとったと仮定した計算をここで行っているが、
				// 「このターン」は最悪ではないが、将来的にはもっと悪くなる行動を敵がとってくる場合も考えられる
				// 例えば、「このターンで敵が連鎖して敵のお邪魔を減らして次のターンで敵にお邪魔が降らない」よりも、
				// このターンはあえて敵が連鎖せずに、次のターンで敵にお邪魔が降ったほうが、その後が敵にとって有利になるケース。
				// その場合は、次のターンで敵にお邪魔が降るケースと、降らないケースのうち、評価値の低いほうを採用するという風に
				// しないと駄目なはず。ただし、計算量が相当増えそうな気がするので後で考えることにする。
				
				// 敵がここで落とす可能性のあるお邪魔の最大数(「その深さで敵が落下させることができるお邪魔の合計」から「ここまでの探索で敵が降らせた可能性のあるお邪魔の数」を引く）
				int enemaydropnum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama - pinfo.enemayojamadropnum;
				// 敵がここで減らす可能性があるスキルポイントの最大数
				int enemayskillminus = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxskillminus - pinfo.enemayskillminusnum;
				// 敵が enemaydropnum のお邪魔を落とす前の相対お邪魔数
				int prevojamanum = pinfo.ojama[0] - pinfo.ojama[1];
				// 敵が enemaydropnum のお邪魔を落とした後の相対お邪魔数
				int nextojamanum = prevojamanum + enemaydropnum;

				// enemaydropnum のお邪魔が落ちることによって、次のターンの状況が変化するかどうか
				// 状況が変化しなくとも、enemaydropnumで一定量お邪魔が落ちてくる場合は、状況が変化するとみなす
				// 自分にお邪魔が落ちてくるようになる場合 または、 敵にお邪魔が落ちなくなる場合を調べる
				// 敵がこれまで落とす可能性のあるお邪魔が 6 個以上の場合、その後にお邪魔を大量に降らすことはほぼ不可能なので、それも考慮する
				// もし、状況が変化する場合は、最大限お邪魔を落とした場合と、そうでない場合の両方を計算し、評価値の低いほうを採用する
				if (parameter.ai3) {
					// 敵が既に6以上お邪魔を降らせた可能性がある場合は、その後お邪魔を降らすことはほぼ不可能なので、enemaydropnumは0とする
					if (pinfo.enemayojamadropnum >= 6) {
						enemaydropnum = 0;
					}
					// 敵のお邪魔落下によって状況が変化する場合か、お邪魔を敵が10以上降らせることができる場合
					else if (enemaydropnum >= 10 || (prevojamanum < 10 && nextojamanum >= 10) || (prevojamanum <= -10 && nextojamanum > -10)) {
						// 2つ目の pinfo をチェックする必要があることを表すフラグを立てる
						checkanotherpinfo = true;
						// お邪魔を落とせるか落とせないかを選択できたターン数の加算
						pinfo.enemayojamadroppedturnnum++;
						// 2つ目のPlayerInfoに（お邪魔を降らせるまえの）pinfoをコピーする
						anotherpinfo = pinfo;
						// お邪魔を落とせるか落とせないかを選択できたターン数の加算
						anotherpinfo.enenotojamadroppedturnnum++;
					}
					else {
						// 状況が変化しない場合は、ここで敵が少量のお邪魔を降らせても良いことは特にないので、降らさないことにする
						enemaydropnum = 0;
					}
				}
				// parameter.ai3 が false の場合は、状況が変化する場合のみ、敵がお邪魔を落とした可能性のあるターン数を増やす
				else if ((prevojamanum < 10 && nextojamanum >= 10) || (prevojamanum <= -10 && nextojamanum > -10)) {
					pinfo.enemayojamadroppedturnnum++;
				}
				// 敵がお邪魔を6以上降らす可能性がある場合、敵がお邪魔を降らせる深さを記録する
				if (enemaydropnum >= 6) {
					pinfo.enemayojamadroppeddepth = depth + 1;
				}
				// 自分のお邪魔を enemaydropnum の分だけ増やす
				pinfo.ojama[0] += enemaydropnum;

				// 敵が落としたお邪魔の数を増やす
				pinfo.enemayojamadropnum += enemaydropnum;
				// 場合によってはマイナスになる場合もある？その場合は0に補正する（これでOK?相手のお邪魔増やさなくても良いか？）
				if (pinfo.ojama[0] < 0) {
					pinfo.ojama[0] = 0;
				}
				// 味方のスキルを減らす。スキルの範囲チェックはこの後でまとめて行う
				pinfo.skill[0] -= enemayskillminus;
				// 敵が減らした可能性のあるスキルポイントすう
				pinfo.enemayskillminusnum += enemayskillminus;
				// 相手がチェインできる可能性がある場合は相手のスキルを増やす
				if (estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].ischain) {
					pinfo.skill[1] += 8;
					// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
					if (pinfo.skill[1] > 100) {
						pinfo.skill[1] = 100;
					}
				}
				// 敵がスキルで消せるブロック数を記録する
				pinfo.eneskilldeleteblocknum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].skilldeleteblocknum;
				// 敵がスキルを使わなかった場合の最小ブロック数を記録する
				pinfo.eneskillnotuseminblocknum = estatusdata.data[0][pinfo.eneojamadroppeddepthbit][depth + 1].minblocknum;
				// 敵がスキルを使わなかったと使った場合の、maxojama の差異を記録する
				pinfo.eneskillnotuseojamadiff = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama - estatusdata.data[0][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama;
				// 敵の落下済のお邪魔の最小値を記録する
				pinfo.minojamanum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].minojamanum;
			}
			// 敵の行動を計算していない場合
			else {
				// スキルで消せるブロック数に暫定値として 2を足しておく todo: これ2で良い？
				pinfo.eneskilldeleteblocknum += 2;
				// チェインしたことにしておく
				pinfo.skill[1] += 8;
				if (pinfo.skill[1] > 100) {
					pinfo.skill[1] = 100;
				}
			}

			// 自分のチェインによる敵のスキルポイントの減少、スキルポイントの範囲チェック、たまっているお邪魔の範囲チェックを行う
			pinfo.calcskillandojama();

			// 次の深さで探索を行う
			if (parameter.ai2) {
				// 次の深さで探索を行い、今回の行動の評価値を取得する
				double hyouka = mysearch(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
				// 敵がお邪魔を降らさない場合の評価値を計算するかどうかのチェック
				if (parameter.ai3 && checkanotherpinfo) {
					// 現在の pinfo をコピーして取っておく
					PlayerInfo pinfobak = pinfo;
					// 敵がお邪魔を降らさない場合のPlayerInfoをpinfoにコピーする
					pinfo = anotherpinfo;
					// スキルなどの計算を行う
					pinfo.calcskillandojama();
					// 評価値を計算する
					double anotherhyouka = mysearch(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
					// 敵がお邪魔を降らした場合と、降らさなかった場合のうち、小さいほうの評価値（相手にとって有利なほう）を採用する
					if (hyouka > anotherhyouka) {
						hyouka = anotherhyouka;
					}
					else {
						// 降らしたほうが評価値が低い場合は、pinfoを元に戻す
						pinfo = pinfobak;
					}
				}
				// 最大評価値の更新処理
				if (maxhyouka < hyouka) {
					maxhyouka = hyouka;
					// 深さが0の場合、bhyouka, bhistory[0]を更新する
					if (depth == 0) {
						bhyouka.total = hyouka;
						bhistory[0] = history[0];
#ifdef DEBUG
						if (parameter.debugmsg) {
							bhistory[1] = pinfo;
						}
#endif
					}
#ifdef DEBUG
					// 深さが ginfo.ai2depth の場合は、その深さの最善手を求めているので、bhistory[depth] に最善手の手を記録する（デバッグ用）
					if (parameter.debugmsg && depth > 0 && depth == ginfo.ai2depth) {
						bhistory[depth] = pinfoorig;
						bhistory[depth + 1] = pinfo;
					}
#endif
				}
				// 深さが0の場合で、ビーム探索の手と同じ手を取った場合の行動の記録
				if (depth == 0) {
					if (x == ginfo.beamx && r == ginfo.beamr) {
						bbeamhyouka.total = hyouka;
						bbeamhistory[0] = history[0];
#ifdef DEBUG
						if (parameter.debugmsg) {
							bbeamhistory[1] = pinfo;
						}
#endif
					}
				}
			}
			// parameter.ai2 が false の場合は、次の深さの探索を行う関数を呼ぶ
			else {
				mysearch(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
			}
		}
		// 深さが0でスレッドを使う場合は、指定されたxの値のみ計算すればよいので、ここで終了
		if (depth == 0 && parameter.usethread) {
			break;
		}
	}
	// 深さが0以外で、置換表を使う場合は、評価値を記録する
	if (parameter.usett && depth != 0) {
		tdata->set(key, maxhyouka);
	}
	return maxhyouka;
}

// 敵の評価値の計算
void search_enemy(int depth, PlayerInfo *history, EnemyStatusData& estatusdata) {
	// この深さのノードのプレーヤーの情報への参照
	PlayerInfo& pinfoorig = history[depth];
	// pinfoorig に対してプレーヤーが行動を行った結果の局面のプレーヤーの情報への参照
	PlayerInfo& pinfo = history[depth + 1];

#ifdef DEBUG
	ginfo.aicount++;
#endif

	// 敵の行動の状態を記録する変数への参照
	// 「前の行動でスキルを使ったかどうか」、「これまでの行動でお邪魔が降ったかどうか」、「現在の深さ」によって記録する場所を変える
	// (depthが0以外の場合は、pinfoorigには、一つ前の深さでの行動の結果（chainなど）が記録されている）
	// (depthが0の場合の計算もするが、実際には使われない）
	EnemyStatus& estat = estatusdata.data[pinfoorig.skillindex][pinfoorig.ojamadroppeddepthbit][depth];
	// チェインが発生するかどうかの記録
	if (pinfoorig.chain > 0) {
		estat.ischain = true;
	}
	// 最大チェイン数と、その時の行動の記録
	if (pinfoorig.chain > estat.maxchain) {
		estat.maxchain = pinfoorig.chain;
		estat.maxchainx = pinfoorig.x;
		estat.maxchainr = pinfoorig.r;
	}
	// これまでに相手に降らせたお邪魔の最大数の記録
	if (pinfoorig.getojama > estat.maxojama) {
		estat.maxojama = pinfoorig.getojama;
	}
	// この深さでスキルで相手に降らせたお邪魔の最大数の記録
	if (pinfoorig.skillusedinthisdepth && pinfoorig.getskillojama > estat.maxskillojama) {
		estat.maxskillojama = pinfoorig.getskillojama;
	}
	// これまで減らした相手のスキルポイントの最大値の記録
	if (pinfoorig.getskillminus > estat.maxskillminus) {
		estat.maxskillminus = pinfoorig.getskillminus;
	}
	// スキルで消せるブロック数の最大値
	int skilldeleteblocknum = pinfoorig.calcskilldeleteblocknum();
	if (skilldeleteblocknum > estat.skilldeleteblocknum) {
		estat.skilldeleteblocknum = skilldeleteblocknum;
	}
	// ブロックの最小値
	int minblocknum = pinfoorig.bb[0].pcount();
	if (minblocknum < estat.minblocknum) {
		estat.minblocknum = minblocknum;
	}
	// お邪魔ブロックの最小値
	int minojamanum = pinfoorig.bb[10].pcount();
	if (minojamanum < estat.minojamanum) {
		estat.minojamanum = minojamanum;
	}

	// お邪魔落下処理
	pinfoorig.ojamadrop(depth);

	// リーフノードまたは、最終ターンを過ぎた場合の処理
	if (depth == ginfo.searchDepth || pinfoorig.turn == MAX_TURN) {
		// 終了する
		return;
	}

	// 置換表を使って同じ局面が出てきた場合は終了する。置換表に評価値（そもそも敵の場合は評価値を計算しない）の値は記録する必要がないので、
	// 置換表内に見つからなかった場合は1（0以外の値）を記録しておく
	TT_DATA *tdata = nullptr;
	Key key;
	if (parameter.usett && depth != 0) {
		key = pinfoorig.calc_enemy_hash();
		bool found;
		tdata = tt.findcache(key, found);
		if (found) {
			return;
		}
		tdata->set(key, 1);
	}

	// この深さでの行動を表す変数への参照（x:落下位置（-1の場合はスキルを使う),r:回転数）
	int8_t& x = pinfoorig.x;
	int8_t& r = pinfoorig.r;
	double maxhyouka = -INF;
	// 可能な行動をすべて試す。ただし、x == -1 は スキル使用を表す
	for (x = -1; x <= 8; x++) {
		// スレッドを使う場合（searchと同じ）以下mysearch()と同様の部分のコメントは省略する
		if (depth == 0 && parameter.usethread) {
			x = pinfoorig.px;
		}
		for (r = 0; r < 4; r++) {
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
			if (!action(pinfoorig, pinfo)) {
				continue;
			}

			// estatus のスキルの使用に関するインデックス番号の計算。初期値として0（スキルを使用していない）を入れておく
			pinfo.skillindex = 0;
			// スキルをこの深さで使用した場合
			if (pinfo.skillusedinthisdepth) {
				pinfo.skillindex = 2;
			}
			// スキルを過去に使用した場合
			else if (pinfo.skillused) {
				pinfo.skillindex = 1;
			}
			// ここまで来たという事はこの深さで生存する行動があるということなので、生存フラグを立てる
			estatusdata.data[pinfo.skillindex][pinfo.ojamadroppeddepthbit][depth + 1].isdead = false;

			// 自分のチェインによる敵のスキルポイントの減少、スキルポイントの範囲チェック、たまっているお邪魔の範囲チェックを行う
			pinfo.calcskillandojama();

			// 次に降ってくるお邪魔の数を記録しておく
			int ojamanum = pinfo.ojama[0];
			// 次の深さで探索を行う
			search_enemy(depth + 1, history, estatusdata);
			// 敵の思考で、お邪魔が降ってなかった場合は、降らす場合も計算する（実際には降ってこないかもしれないがそのことはわからないので計算しておく）
			// ただし、初手のみ味方の最大連鎖数は計算済なので、それでお邪魔が絶対降ってこない場合は飛ばす
			if (depth == 0 && pinfo.ojama[0] - pinfo.ojama[1] + chain_ojamatable[ginfo.myfirstmaxchain] < 10) {
				continue;
			}
			if (ojamanum < 10) {
				pinfo.ojama[0] = 10;
				search_enemy(depth + 1, history, estatusdata);
			}
		}
		// スレッドを使用している場合は、深さ0の場合はここで終了する
		if (depth == 0 && parameter.usethread) {
			break;
		}
	}
}

// ai のスレッド使用バージョン
// 深さ0における x の行動（-1～8）をスレッドに分けて探索する
void ai_thread() {
	// 下記のデータはスレッド毎に計算し、計算終了後にそれぞれの最高値を採用する
	EnemyStatusData enemystatusdata_thread[11];
	PlayerInfo thread_history[11][MAX_DEPTH + 1];
	PlayerInfo thread_besthistory[11][MAX_DEPTH + 1];
	PlayerInfo thread_bestbeamhistory[11][MAX_DEPTH + 1];
	HyoukaData besthyouka_thread[11];
	HyoukaData bestbeamhyouka_thread[11];
	std::vector<std::thread> threads;

	// parameter.ai2 が true の場合に、どの深さから探索を行うかを表す変数。
	// 通常は深さ0から探索するので、0を入れておく
	ginfo.ai2depth = 0;
	// 深さ0の各xの行動に対して一つずつスレッドを割り当てる
	// x = -1 から始まるので、インデックスは x + 1 を使う
	for (int x = -1; x <= 8; x++) {
		// 初期値を設定する
		thread_history[x + 1][0] = history[0];
		PlayerInfo& pinfoorig = thread_history[x + 1][0];
		// 評価（ビーム探索の場合も）を－∞にしておく
		besthyouka_thread[x + 1].total = -INF;
		bestbeamhyouka_thread[x + 1].total = -INF;
		// px に深さ0で取る行動を入れておく
		pinfoorig.px = x;
		// 各 x に対するスレッドを呼び出す
		if (!ginfo.isenemy) {
			// 自分の探索の場合はスレッドを呼び出す
			threads.emplace_back(mysearch, 0, thread_history[x + 1], thread_besthistory[x + 1], thread_bestbeamhistory[x + 1], std::ref(besthyouka_thread[x + 1]), std::ref(bestbeamhyouka_thread[x + 1]), std::ref(enemystatusdata));
		}
		else {
			// 敵の場合は、enemystatusdata を計算するのが目的なので、実行前にその内容を初期化しておく
			enemystatusdata_thread[x + 1].clear(ginfo.searchDepth);
			threads.emplace_back(search_enemy, 0, thread_history[x + 1], std::ref(enemystatusdata_thread[x + 1]));
		}
	}
	// 全スレッドの処理が終了するまで待つ
	for (auto& t : threads) {
		t.join();
	}

	// 各スレッドの計算結果のうち、最高評価値の行動を探す
	double maxbesthyouka = -INF;
	double maxbestbeamhyouka = -INF;
	for (int x = -1; x <= 8; x++) {
		// 自分の場合
		if (!ginfo.isenemy) {
			if (!parameter.ai2) {
				if (maxbesthyouka < besthyouka_thread[x + 1].total) {
					besthyouka = besthyouka_thread[x + 1];
					maxbesthyouka = besthyouka.total;
					memcpy(besthistory, thread_besthistory[x + 1], sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
				}
				if (maxbestbeamhyouka < bestbeamhyouka_thread[x + 1].total) {
					bestbeamhyouka = bestbeamhyouka_thread[x + 1];
					maxbestbeamhyouka = bestbeamhyouka.total;
					memcpy(bestbeamhistory_log, thread_bestbeamhistory[x + 1], sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
				}
			}
			else {
				if (maxbesthyouka < besthyouka_thread[x + 1].total) {
					besthyouka = besthyouka_thread[x + 1];
					maxbesthyouka = besthyouka.total;
//					besthistory[0] = thread_besthistory[x + 1][0];
					memcpy(besthistory, thread_besthistory[x + 1], sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
//					besthistory[0].dump_field();
				}
				if (maxbestbeamhyouka < bestbeamhyouka_thread[x + 1].total) {
					bestbeamhyouka = bestbeamhyouka_thread[x + 1];
					maxbestbeamhyouka = bestbeamhyouka.total;
					memcpy(bestbeamhistory_log, thread_bestbeamhistory[x + 1], sizeof(PlayerInfo) * (ginfo.searchDepth + 1));
				}
			}
		}
		// 敵の場合は、enemystatusdata を計算する
		else {
			// スキルの場合は、一つしか計算していないので、まずそれを採用する
			if (x == -1) {
				enemystatusdata = enemystatusdata_thread[0];
			}
			// それ以外の場合
			else {
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < (1 << ginfo.searchDepth); j++) {
						for (int k = 1; k <= ginfo.searchDepth; k++) {
							EnemyStatus& estat1 = enemystatusdata.data[i][j][k];
							EnemyStatus& estat2 = enemystatusdata_thread[x + 1].data[i][j][k];
							estat1.ischain |= estat2.ischain;
							estat1.isdead &= estat2.isdead;
							if (estat1.maxchain < estat2.maxchain) {
								estat1.maxchain = estat2.maxchain;
								estat1.maxchainx = estat2.maxchainx;
								estat1.maxchainr = estat2.maxchainr;
							}
							if (estat1.maxojama < estat2.maxojama) {
								estat1.maxojama = estat2.maxojama;
							}
							if (estat1.maxskillminus < estat2.maxskillminus) {
								estat1.maxskillminus = estat2.maxskillminus;
							}
							if (estat1.maxskillojama < estat2.maxskillojama) {
								estat1.maxskillojama = estat2.maxskillojama;
							}
							if (estat1.skilldeleteblocknum < estat2.skilldeleteblocknum) {
								estat1.skilldeleteblocknum = estat2.skilldeleteblocknum;
							}
							if (estat1.minblocknum > estat2.minblocknum) {
								estat1.minblocknum = estat2.minblocknum;
							}
							if (estat1.minojamanum > estat2.minojamanum) {
								estat1.minojamanum = estat2.minojamanum;
							}
						}
					}
				}
			}
		}
	}
}

void ai() {
	if (parameter.usethread) {
		ai_thread();
	}
	else {
		if (!ginfo.isenemy) {
			mysearch(0, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		else {
			search_enemy(0, history, enemystatusdata);
		}
	}
	// 自分の場合で、parameter.ai2 が true の場合に、最善手のデバッグ表示を行いたい場合の処理
#ifdef DEBUG
	if (!ginfo.isenemy && parameter.debugmsg && parameter.ai2) {
		// デバッグ用なので、置換表は使わないことにする（時間を気にしなくてよいので）
		bool usettbak = parameter.usett;
		parameter.usett = false;
		// besthistory のバックアップ用の領域
		PlayerInfo besthistorybak[MAX_DEPTH + 1];

		// 深さ0の最善手はわかっているので、その行動をとった状態で、最善手を探せば深さ1の最善手がわかる
		// 同様にその後、深さ1の最善手を取った状態で最善手を探せば深さ2の最善手がわかる・・・以下同様
		// 深さ1から順に最善手を求める。ginfo.ai2depthに探索開始時の深さを設定する
		for (ginfo.ai2depth = 1; ginfo.ai2depth < ginfo.searchDepth; ginfo.ai2depth++) {
			// 既に分かっている深さginfo.ai2depthの行動をとらせた状態で ai を呼び最善手を探す
			history[ginfo.ai2depth] = besthistory[ginfo.ai2depth];
			mysearch(ginfo.ai2depth, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		// 最善手のhistoryのバックアップを取っておく
		memcpy(besthistorybak, besthistory, sizeof(PlayerInfo) * (MAX_DEPTH + 1));

		// ビーム探索の行動をとった場合の最善手のhisoryを同様の方法で計算する
		history[0] = bestbeamhistory_log[0];
		besthistory[1] = bestbeamhistory_log[1];
		for (ginfo.ai2depth = 1; ginfo.ai2depth < ginfo.searchDepth; ginfo.ai2depth++) {
			history[ginfo.ai2depth] = besthistory[ginfo.ai2depth];
			mysearch(ginfo.ai2depth, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		// bestbeam_history_logにbesthistory をコピーする。
		memcpy(bestbeamhistory_log + 1, besthistory + 1, sizeof(PlayerInfo) * (MAX_DEPTH + 1 - 2));
		// besthistoryにコピーしておいた情報を書き戻す。これらはmainに戻った後でダンプされる
		memcpy(besthistory, besthistorybak, sizeof(PlayerInfo) * (MAX_DEPTH + 1));
		// 置換表を使うかどうかのパラメータを元に戻す
		parameter.usett = usettbak;
	}
#endif
}

vector <PlayerInfo, MyAllocator<PlayerInfo>> beam_pdata[MAX_BEAM_DEPTH][MAX_CHAIN + 1];
PlayerInfo firstbestbeamhistory[MAX_BEAM_DEPTH];
PlayerInfo bestbeamhistory[MAX_BEAM_DEPTH];

// bpdata に格納されている PlayerData に対して先頭から size 個までを取り出して、
// それぞれに対して取りうる行動を行った結果を beam_pdata[depth + 1][chain] に格納する（chain は　行動をとった結果が pinfo の場合、pinfo.hyouka.chainを使う）
// bpdata のデータは、この関数を呼び出す前に評価値順にソートしておく必要がある
// 探索は maxnum 個まで
void beamsearch_pdata(vector<PlayerInfo, MyAllocator<PlayerInfo>>& bpdata, int depth, size_t size, size_t maxnum, int beamdropdepth, int beamfirstdropnum, bool isskill) {
	// 行動の結果の PlayerInfo を格納する変数
	PlayerInfo pinfo;
	// 探索した bpdata の数
	int count = 0;
	// bpdata から一つずつ順にデータを取り出す
	for (auto& pinfoorig : bpdata) {
		// 時間切れになった場合か、maxnum個探索した時点で終了する
		if (ginfo.t.time() >= parameter.firsttimelimit || count > maxnum) {
			if (count <= maxnum) {
				ginfo.istimeout = true;
			}
			break;
		}

		int8_t &x = pinfoorig.x;
		int8_t &r = pinfoorig.r;
		// pinfoorig に対して、取りうるすべての行動の結果を計算する。
		// ただし、探索において、スキルは使わないので x は 0 から始める
		for (x = parameter.beamminx; x <= parameter.beammaxx; x++) {
			for (r = 0; r < 4; r++) {
				// 深さが0で、ginfo.checkonlybeamxr が true の場合は、ビーム探索で得られた行動に対してのみ探索を行いたいので、それ以外の行動は飛ばす
				if (depth == 0 && ginfo.checkonlybeamxr && (x != ginfo.beamx || r != ginfo.beamr)) {
					continue;
				}
				// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
				if (!action(pinfoorig, pinfo)) {
					continue;
				}

				// 次の深さが beamdropdepth に等しい場合は、beamfirstdropnum の分だけ自分のお邪魔を増やす
				if (depth + 1== beamdropdepth && beamfirstdropnum > 0) {
					pinfo.ojama[0] += beamfirstdropnum;
				}
				// 自分のチェインによる敵のスキルポイントの減少、スキルポイントの範囲チェック、たまっているお邪魔の範囲チェックを行う
				pinfo.checkojama();
//				pinfo.calcskillandojama();
				// お邪魔はここで落としておく
				pinfo.ojamadrop(depth + 1);

				// 置換表に登録されている場合は飛ばす
				if (parameter.usett) {
					TT_DATA *tdata;
					Key key = pinfo.calc_beam_hash();
					bool found;
					tdata = tt.findcache(key, found);
					if (found) {
						continue;
					}
					tdata->set(key, 1);
				}

				// 連鎖を探索している場合は、calcbeamhyouka() を実行して評価値を計算する
				if (!isskill) {
					pinfo.calcbeamhyouka();
				}
				// そうでなければ calcbeamhyouka_skill() を実行して評価値を計算する
				else {
					pinfo.calcbeamhyouka_skill();
				}
#ifdef DEBUG
				ginfo.aicount += 36;
#endif
				// 一つ前の行動として、px, pr に x, r を格納しておく
				pinfo.px = x;
				pinfo.pr = r;
				// beam_pdata[depth + 1][pinfo.hyouka.chain] に pinfo を追加する
				// スキル探索の場合の chain はスキルで消せるブロック数
				// この部分は排他制御を行わないとまずいので、ミューテックスを使う
				if (parameter.usethread) {
					std::lock_guard<std::mutex> lock(mtx);
					beam_pdata[depth + 1][pinfo.hyouka.chain].push_back(pinfo);
				}
				// スレッドを使わない場合は排他制御は必要ない
				else {
					beam_pdata[depth + 1][pinfo.hyouka.chain].push_back(pinfo);
				}
			}
		}
		// countを増やす
		count++;
	}
}

// 大連鎖または、スキル発動を狙うためのビーム探索
// 返り値として、探索時に見つかった最大チェーン数を返す（スキル発動を狙っている場合は、この返り値は無意味）
// 最初のPlayerDataは　history[0] に入っているものとする
// ビーム探索は以下の方法で行う。
// ・多様性を確保するため、深さ depth における PlayerData に対する全行動の結果は、その際のチェイン数毎に 
//   pdata[chain][depth + 1] の vector に保存する
// ・深さ depth における探索は、pdata[chain][depth] から、チェイン数の大きい順に、parameter.beamChainWidth ずつ、
//   合計が parameter.beamWidth になるまで取ってくるものとする
// ・探索は、まず、parameter.beamchainmax 以上のチェインが見つかるまで行う（parameter.beamchainmaxが負の場合はこの処理は行わない）
//   見つかった場合は見つかった次の深さの pdata[chain][depth + 1] のうち、parameter.beamchainmax + parameterbeamchainminus よりチェイン数が少ないものはすべて削除し、探索を続ける
//   こうすることにより、この探索により、最も早く parameter.beamchainmax + parameterbeamchainminus を達成可能なものだけを残すことができる
//   (長期的にみて、大連鎖を組めたとしても、その途中で大連鎖が組めていなければ、先に相手が大連鎖を組んできたときに対抗できない）
//   また、parameter.stopbeamChainmax が　true の場合は、そこで探索を打ち切る
// ・parameter.beamchainmax2 以上のチェインが見つかるか、深さが parameter.beamDepth になるか、計算時間が parameter.firsttimelimt になるまで続ける
//　 (ただし、parameter.beamchainmax2が負の場合は最初の条件は無視する）
//   時間切れになった場合は、探索が完了している深さの情報を使う
// 探索後は、最深部の探索結果の中から最も評価値の高いものを探して bestbeamhistory にその情報を格納する
//   この際に、ソートの処理などで多少の時間がかかるので、もっとも評価値の高いものを探すのは、parameter.beamtimelimit までとする
// beamdropdepth と同じ depth において、beamfirstdropnum の数だけお邪魔を降らす
// isskill が true の場合はスキル発動を狙う
// 探索の結果、以下の ginfo の値を設定する
//   beamchainmaxdepth: parameter.beamchainmax 以上の連鎖が行われた最初の深さ（なければ探索が行われた最大の深さが入る）
//   eachchaindepth[]:	インデックスが表すチェイン数を最初に達成した深さ（達成しない場合は1000を代入する）
// 　istimeout:			時間切れになった場合は true にする
//   beamDepth			実際に行った探索の深さ

int beamsearch(int beamdropdepth = 0, int beamfirstdropnum = 0, bool isskill = false) {
	// 時間切れでないことにする
	ginfo.istimeout = false;
	// 置換表の初期化
	if (parameter.usett) {
		tt.clear();
	}
	// 各チェイン数を達成した最小の深さを1000に設定する
	for (int i = 0; i <= MAX_CHAIN; i++) {
		ginfo.eachchaindepth[i] = 1000;
	}
	// 最大チェイン数の初期化
	int maxchain = 0;
	// 最大チェイン数を達成した深さの初期化
	int maxchaindepth = 0;
	// 深さ 0 に登録されている PlayerData をクリアする
	for (int i = 0; i < MAX_CHAIN; i++) {
		beam_pdata[0][i].clear();
	}
	// 最初のPlayerDataに対してお邪魔を降らせる
	history[0].ojamadrop(0);
	// 深さ 0 のチェイン数が 0 の vector に に最初のPlayerDataを登録する
	beam_pdata[0][0].push_back(history[0]);
	// 深さ、探索幅の合計、チェイン毎の探索幅 をパラメータから、ginfoの変数にコピーする
	ginfo.beamDepth = parameter.beamsearchDepth;
	ginfo.beamWidth = parameter.beamsearchWidth;
	ginfo.beamChainWidth = parameter.beamsearchChainWidth;
	// 探索中の深さを格納する変数
	int depth;
	// beamChainmax以上のチェインが見つかっているかどうかを表す変数の初期化
	bool foundbeamchainmax = false;
#ifdef DEBUG
	if (parameter.debugmsg) {
		cerr << endl;
	}
#endif
	// 深さ 0 から順に、ginfo.beamDepth の深さになるまで探索を繰り返す
	for (depth = 0; depth < ginfo.beamDepth; depth++) {
		// 時間切れになった場合は、ginfo.istimeout を true にしてループを抜ける
		if (ginfo.t.time() >= parameter.firsttimelimit) {
			ginfo.istimeout = true;
			break;
		}
		// この深さの行動の結果を格納する全 vector の初期化
		for (int i = 0; i < MAX_CHAIN; i++) {
			beam_pdata[depth + 1][i].clear();
		}
		// スレッドを格納する vector
		std::vector<std::thread> threads;
		// この深さで行う探索の残り数を ginfo.beamWidth にセットする
		size_t leftnum = ginfo.beamWidth;
		// チェイン数が大きい順に beam_pdata[depth][chain] から PlayerData 最大 ginfo.beamChainWidth だけ取ってきて探索を行う
		// chainごとにスレッドを立ち上げて並列処理を行う
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			// 一つもなかった場合は次へ
			size_t size = beam_pdata[depth][chain].size();
			if (size == 0) {
				continue;
			}
			// 時間切れになった場合や、探索の残り数がなくなった場合はループを抜ける
			if (ginfo.t.time() >= parameter.firsttimelimit || leftnum <= 0) {
				// 時間切れの場合は時間切れフラグを立てる
				if (leftnum > 0) {
					ginfo.istimeout = true;
				}
				break;
			}
			// この深さでの最大探索数を計算する
			size_t maxnum = size;
			if (maxnum > ginfo.beamChainWidth) {
				maxnum = ginfo.beamChainWidth;
			}
			if (maxnum > leftnum) {
				maxnum = leftnum;
			}
			// 残り探索数を引く
			leftnum -= maxnum;
			int chain_count = 0;
			// beam_pdata[depth][chain] に格納されている PlayerData を評価値の順にソートする
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());

			// これいる？
			//if (size >= ginfo.beamChainWidth * 1.2) {
			//	beam_pdata[depth][chain].resize(static_cast<int>(ginfo.beamChainWidth * 1.2));
			//	beam_pdata[depth][chain].shrink_to_fit();
			//}

			if (parameter.usethread) {
				// beam_pdata[depth][chain] に対する探索を行うスレッドを起動する
				threads.emplace_back(beamsearch_pdata, std::ref(beam_pdata[depth][chain]), depth, size, maxnum, beamdropdepth, beamfirstdropnum, isskill);
			}
			else {
				beamsearch_pdata(beam_pdata[depth][chain], depth, size, maxnum, beamdropdepth, beamfirstdropnum, isskill);
			}
		}

		// 全スレッドが終了するまで待つ
		if (parameter.usethread) {
			for (auto& t : threads) {
				t.join();
			}
		}

		// 最大の深さの情報をデバッグ表示させるためのフラグ（デバッグ用）
#ifdef DEBUG
		bool isfirst = true;
#endif
		// この深さで行った探索の結果生成された PlayerData の数を数える変数の初期化
		size_t count = 0;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
#ifdef DEBUG
			// depth の深さの各チェイン数の beam_pdata[depth][chain] の情報のデバッグ表示
			if (beam_pdata[depth][chain].size() > 0) {
				// parameter.turndebugmsg が true の場合は、最大チェインのみ、
				// parameter.debugmsg が true の場合は全チェインについて表示する
				// ただし、表示するのはビーム探索の性能評価を行う parameter.checkbeamchain が false で、最初のターンのみ
				if (((isfirst && parameter.turndebugmsg) || parameter.debugmsg) && !parameter.checkbeamchain && ginfo.turn == 0) {
					auto pinfoorig = beam_pdata[depth][chain].begin();
					// 深さ、チェイン数、探索で生成したスレッドのサイズ、beam_pdata[depth][chain]のサイズ、先頭の評価値、先頭のチェイン数、先頭の通常のブロック数、計算時間 を表示する
					cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " t " << setw(2) << threads.size() << " cc " << setw(3) << beam_pdata[depth][chain].size() << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig->hyouka.total << " " << defaultfloat << setw(4) << static_cast<int>(pinfoorig->hyouka.chain) << " " << setw(4) << static_cast<int>(pinfoorig->hyouka.normalblocknum) << " " << setw(5) << ginfo.t.time() << " ms " << endl;
					isfirst = false;
				}
			}
#endif
			// この深さで行った探索の結果を格納した beam_pdata[depth + 1][chain] を調べ、その合計数を count に計算し、
			// maxchain と maxchaindepth と ginfo.eachchaindepth を更新する、
			if (beam_pdata[depth + 1][chain].size() > 0) {
				count += beam_pdata[depth + 1][chain].size();
				if (chain > maxchain) {
					maxchain = chain;
					maxchaindepth = depth + 1;
				}
				if (ginfo.eachchaindepth[chain] > depth + 1) {
					ginfo.eachchaindepth[chain] = depth + 1;
				}
			}
		}

		// parameter.beamChainmax 以上のチェインを初めて達成した場合の処理
		if (!foundbeamchainmax && parameter.beamChainmax > 0 && maxchain >= parameter.beamChainmax) {
			// ginfo.beamchainmaxdepth にその深さを記録する
			ginfo.beamchainmaxdepth = depth + 1;
			// このパラメータが true の場合は、ここで探索を打ち切る
			if (parameter.stopbeamChainmax) {
				depth++;
				break;
			}
			// 見つかったのでフラグを true にする
			foundbeamchainmax = true;
			// チェイン数が、parameter.beamChainmax + parameter.beamChainmaxminus 未満の次の beam_pdata のデータを削除する
			for (int i = 0; i < parameter.beamChainmax + parameter.beamChainmaxminus; i++) {
				beam_pdata[depth + 1][i].clear();
			}
		}
		// parameter.beamChainmax2 が正の場合で、 maxchain が parameter.beamChainmax2 以上になっていた場合はここで探索を打ち切る
		if (parameter.beamChainmax2 > 0 && maxchain >= parameter.beamChainmax2) {
			depth++;
			break;
		}
		// この深さの探索の結果、新しい PlayerData が一つも存在しない場合は、探索を打ち切る（現在の depth の探索結果を採用するので、depth は増やさない）
		if (count == 0) {
			break;
		}
	}
	// 実際に探索を行った深さを ginfo.beamDepth に記録する
	ginfo.beamDepth = depth;
	// beam_pdata[depth][chain] の中から最も評価値の高いものを探し、 bestbeamhistory に記録する
	double maxhyouka = -INF;
	// チェーン数の大きい順に調べる
	for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
		if (beam_pdata[depth][chain].size() > 0) {
			// 評価値の順にソートする（よく考えたらソートせず、一つ一つチェックしたほうが速いのでは？）
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());
			// 先頭の最も評価値の高い PlayerData の評価値をしらべ、最大の評価値の PlayerData を更新する
			if (beam_pdata[depth][chain].begin()->hyouka.total > maxhyouka) {
				bestbeamhistory[depth] = *beam_pdata[depth][chain].begin();
				maxhyouka = bestbeamhistory[depth].hyouka.total;
			}
			// maxchain, maxchaindepth, ginfo. eachchaindepth[chain] の更新（これ必要？）
			if (chain > maxchain) {
				maxchain = chain;
				maxchaindepth = depth;
			}
			if (ginfo.eachchaindepth[chain] > depth) {
				ginfo.eachchaindepth[chain] = depth;
			}
			// sort に時間がかかるので、 parameter.beamtimelimit を過ぎた時点で打ち切る
			if (ginfo.t.time() >= parameter.beamtimelimit) {
				ginfo.istimeout = true;
				break;
			}
		}
	}

	// parameter.beamChainmax 以上のチェーンが見つからなかった場合は、行った探索の深さを ginfo.beamchainmaxdepth に入れておく
	if (!foundbeamchainmax && parameter.beamChainmax > 0) {
		ginfo.beamchainmaxdepth = depth;
	}
	// みつかった最大チェイン数を ginfo.maxchaindepth に記録する
	ginfo.maxchaindepth = maxchaindepth;

	// デバッグ表示
	if (!parameter.checkbeamchain) {
#ifdef DEBUG
		if (parameter.turndebugmsg) {
			if (ginfo.turn == 0) {
				cerr << "depth " << setw(2) << ginfo.beamDepth << " c " << setw(3) << maxchain << " hyouka " << fixed << setprecision(2) << setw(7) << bestbeamhistory[depth].hyouka.total << " " << defaultfloat << setw(4) << static_cast<int>(bestbeamhistory[depth].hyouka.chain) << " " << setw(4) << static_cast<int>(bestbeamhistory[depth].hyouka.normalblocknum) << " " << setw(5) << ginfo.t.time() << " ms" << endl;
			}
			else {
				cerr << "bd " << ginfo.beamDepth << " mch " << maxchain << "/" << maxchaindepth << " t " << setw(5) << ginfo.t.time() << " ";
			}
		}
#endif
	}
	else {
		// こちらはゲーム本番ではなく、ビーム探索の性能を評価するパラメータを true にした場合のデバッグ表示
#ifdef DEBUG
		cout << depth << "\t" << static_cast<int>(bestbeamhistory[depth].hyouka.chain) << "\t" << maxchain << "\t" << maxchaindepth << "\t" << ginfo.beamchainmaxdepth << "\t";
		cerr << depth << "\t" << static_cast<int>(bestbeamhistory[depth].hyouka.chain) << "\t" << maxchain << "\t" << maxchaindepth << "\t" << ginfo.beamchainmaxdepth << "\t";
#endif
		bestbeamhistory[depth].hyouka.maxdepth = ginfo.beamchainmaxdepth;
		return depth;
	}

	// bestbeamhistory[depth] の prev を順にたどっていくことで、深さ 0 から bestbeamhistory[depth] に到達するまでの手順を bestbeamhistory に計算する
	// bestbeamhistory[0]の情報は必ず必要なので、この計算は必要
	PlayerInfo *ptr = bestbeamhistory[depth].prev;
	while (ptr) {
		depth--;
		bestbeamhistory[depth] = *ptr;
		ptr = ptr->prev;
	}
#ifdef DEBUG
	// 上記の結果のデバッグ表示
	if (parameter.debugmsg) {
		dump_fields(bestbeamhistory, ginfo.beamDepth, true);
	}
#endif
	// 探索時に見つかった最大チェーン数を結果として返す
	return maxchain;
}

// ビーム探索の性能を測るためのデバッグ用関数
void checkbeamchain() {
	Timer t;
	double counter = 0;
	double chaintotal = 0;
	double chaindepthtotal = 0;
	double maxchaindepthtotal = 0;
	double timetotal = 0;
	ginfo.read_start_info();
	while (true) {
		cerr << counter << "\t";
		cout << counter << "\t";
		t.restart();
		history[0] = ginfo.pinfo[0];
		parameter.beamChainmax = parameter.beamChainmax1;
		// もう、depthを返さない！要修正
		int depth = beamsearch();
		chaintotal += bestbeamhistory[depth].hyouka.chain;

		chaindepthtotal += depth;
		maxchaindepthtotal += bestbeamhistory[depth].hyouka.maxdepth;
		timetotal += t.time();
		counter++;
		cerr << t.time() << "\t" << chaintotal / counter << "\t" << chaindepthtotal / counter << "\t" << maxchaindepthtotal / counter << "\t" << timetotal / counter << endl;
		cout << t.time() << "\t" << chaintotal / counter << "\t" << chaindepthtotal / counter << "\t" << maxchaindepthtotal / counter << "\t" << timetotal / counter  << endl;
		while (!ginfo.read_start_info()) {
			if (cin.eof()) {
				return;
			}
		}
	}
}

// 各ターンの各種データを記録する変数（デバッグ用）
TurnData turndata;

// メイン関数
int main(int argc, char *argv[]) {
	//cerr << sizeof(PlayerInfo) << "," << sizeof(BitBoard) << "," << sizeof(HyoukaData) << "," << sizeof(bool) << endl;
	int dtime = 0;

	// タイマーのリセット
	ginfo.t.restart();
	// 大連鎖を目指すか、スキル発動を目指すかを表す変数をパラメータから設定
	bool& skillmode = ginfo.skillmode;
	skillmode = parameter.firstskillmode;
	// 前のターンのスコア
	int prevscore[2] = { 0, 0 };
	// 前のターンでお互いにお邪魔が降ってこなかった場合の、このターンのフィールド上のお邪魔の数(自分と敵）
	int assumed_droppedojamanum[2] = { 0, 0 };
	// 前のターンでお互いにお邪魔が降ってこなかった場合の落下予定のお邪魔の数
	int assumed_ojamanum[2] = { 0, 0 };
	// 終了までの時間を記録するタイマー
	Timer totaltime;
	// 汎用のタイマー
	Timer t;
	// ゲーム番号（デバッグ時に、複数のゲーム情報が入ったデータを読み込んだ場合、どのゲームのデータを処理しているかを表す）
	int gamenum = 0;
	// パラメータを解釈する
	parameter.parseparam(argc, argv);
	// 初期化処理を呼ぶ（かかった時間を記録する）
	turndata.inittime = init();

	// AIの名前を出力する
	cout << "ysai" << endl;
	cout.flush();

	// デバッグ用の、ビーム探索の性能の計測を行うパラメータが設定されている場合はその関数を読んで終了
	if (parameter.checkbeamchain) {
		checkbeamchain();
		return 0;
	}

	// デバッグ時に、複数のゲームデータが入ったファイルを読み込んだ場合、parameter.checkgame で指定したゲームの
	// データを読み込むための処理
	if (parameter.checkgame > 0) {
		while (parameter.checkgame != gamenum) {
			// ゲーム開始の情報を読み込む
			ginfo.read_start_info();
			// 次のゲーム開始の情報まで飛ばす
			while (!ginfo.read_start_info());
			// ここまでで、一つ分のゲームのデータを読み込んで飛ばしたことになる
			gamenum++;
		}
	}
	else {
		// 初期情報の読み込み
		ginfo.read_start_info();
	}
	// ビーム探索が行われているかどうかを表すフラグ
	bool& beamflag = ginfo.beamflag;
	beamflag = false;
	// 最初に行うビーム探索で得られた大連鎖行動の深さ
	int firstbeamdepth = 0;
	// 処理した最終ターン（デバッグ用）
	int lastturn = 0;

	// ターン情報を読み込む
	// ターン情報が得られる限り繰り返す
	while (ginfo.read_turn_info()) {
		// このターンで関数 ai を呼んだ回数を記録する変数の初期化
#ifdef DEBUG
		ginfo.aicount = 0;
#endif
		// ビーム探索において、初手が ginfo.beamx, ginfo.beamr の行動のみを探索するかどうかを表すフラグを false にしておく
		ginfo.checkonlybeamxr = false;
		// 特定のターン以外を無視する処理（デバッグ用）
		if (parameter.checkturn >= 0 && ginfo.turn != parameter.checkturn) {
			// assumed 関連の計算のみしておく
			for (int i = 0; i < 2; i++) {
				prevscore[i] = ginfo.pinfo[i].score;
				assumed_droppedojamanum[i] = ginfo.pinfo[i].bb[10].pcount();
				if (ginfo.pinfo[0].ojama[i] >= 10) {
					assumed_ojamanum[i] = ginfo.pinfo[0].ojama[i] - 10;
					assumed_droppedojamanum[i] += 10;
				}
			}
			continue;
		}

		// 特定のターンの、連鎖を確認するための処理（デバッグ用）
		// parameter.checkturn において、parameter.beamx, parameter.beamr の行動をとった時の連鎖をデバッグ表示する
		if (parameter.checkturn >= 0 && ginfo.turn == parameter.checkturn && parameter.showchain) {
			PlayerInfo pinfo;
			history[0] = ginfo.pinfo[0];
			history[0].x = parameter.beamx;
			history[0].r = parameter.beamr;
			action(history[0], pinfo);
			// parameter.beamr2 が 0以上の場合は、2爪の行動についてもデバッグ表示する
			if (parameter.beamr2 >= 0) {
				history[0] = pinfo;
				history[0].x = parameter.beamx2;
				history[0].r = parameter.beamr2;
				action(history[0], pinfo);
			}
			return 0;
		}

		// beam_pdata のメモリを開放する
		for (int i = 0; i < MAX_BEAM_DEPTH; i++) {
			for (int j = 0; j <= MAX_CHAIN; j++) {
				beam_pdata[i][j].clear();
				beam_pdata[i][j].shrink_to_fit();
			}
		}

		// ターン数と残り思考時間のデバッグ表示
#ifdef DEBUG
		if (parameter.turndebugmsg) {
			cerr << endl << "Turn " << ginfo.turn << " " << ginfo.pinfo[0].timeleft << " ms " << totaltime.time() << " ";
		}
		if (parameter.debugmsg) {
			cerr << endl;
		}
#endif

		// 最初のターンでなければ、最初のターンのビーム探索を行うかどうかを表すフラグを false にする
		if (ginfo.turn > 0) {
			ginfo.firstbeamcheck = false;
		}

		// お互いのフィールドの死亡確定お邪魔数を計算する
		ginfo.pinfo[0].calcdeadojamanum();
		ginfo.pinfo[1].calcdeadojamanum();
		// 敵のお邪魔とスキルと死亡確定お邪魔数をそれぞれ設定する
		ginfo.pinfo[0].ojama[1] = ginfo.pinfo[1].ojama[0];
		ginfo.pinfo[1].ojama[1] = ginfo.pinfo[0].ojama[0];
		ginfo.pinfo[0].skill[1] = ginfo.pinfo[1].skill[0];
		ginfo.pinfo[1].skill[1] = ginfo.pinfo[0].skill[0];
		ginfo.pinfo[0].deadojamanum[1] = ginfo.pinfo[1].deadojamanum[0];
		ginfo.pinfo[1].deadojamanum[1] = ginfo.pinfo[0].deadojamanum[0];
		// 最初のターンの敵のスキルで消せる数を計算
		ginfo.pinfo[0].eneskilldeleteblocknum = ginfo.pinfo[1].calcskilldeleteblocknum();
		// 最初のターンの敵のフィールド内のお邪魔の数
		ginfo.pinfo[0].minojamanum = ginfo.pinfo[0].firstenedroppedojamanum = ginfo.pinfo[1].bb[10].pcount();

		// そのターンの初期フィールドのデバッグ表示
#ifdef DEBUG
		if (parameter.debugmsg) {
			cerr << endl;
			dump_fields(ginfo.pinfo, 1);
		}
#endif

		// お互いの1手目の最大連鎖数を計算する
		// お邪魔を降らせるのでコピーする（この後 pinfo を ginfo.pinfo[0] の代わりに使ってはいけない！）
		PlayerInfo pinfo = ginfo.pinfo[0];
		pinfo.ojamadrop(0);
		// calcbeamhyoukaを行うと、その状態でとれるすべての行動に対する最大連鎖数を計算し、pinfo.hyouka.chain に格納する
		pinfo.calcbeamhyouka();
		PlayerInfo eneinfo = ginfo.pinfo[1];
		eneinfo.ojamadrop(0);
		eneinfo.calcbeamhyouka();
		ginfo.myfirstmaxchain = pinfo.hyouka.chain;
		ginfo.enefirstmaxchain = eneinfo.hyouka.chain;
#ifdef DEBUG
		// デバッグ表示
		if (parameter.turndebugmsg) {
			cerr << "mc " << ginfo.myfirstmaxchain << "/" << ginfo.enefirstmaxchain << " ";
		}
#endif
		// 状況が変化した場合に、スキル発動を目指すモードを解除する（この後で改めて連鎖を目指すかスキル発動を目指すかを計算しなおす）
		// 状況が変化するとは以下の場合を表す
		// １．どちらかがお邪魔を送った結果、お互いの落下予定のお邪魔の数 / 10(切り捨て）が変化した
		// ２．どちらかがお邪魔を送った結果、お互いのフィールドにたまっているお邪魔の数が予定と変化した
		// ３．いずれかの点数が10点以上増加した（お互いが同時に同程度のお邪魔を送った場合、上記の１，２のいずれも満たさないケースがあるため）
		if (assumed_ojamanum[0] / 10 != ginfo.pinfo[0].ojama[0] / 10 || assumed_ojamanum[1] / 10 != ginfo.pinfo[0].ojama[1] / 10 ||
			assumed_droppedojamanum[0] != ginfo.pinfo[0].bb[10].pcount() || assumed_droppedojamanum[1] != ginfo.pinfo[1].bb[10].pcount() ||
			prevscore[0] + 10 <= ginfo.pinfo[0].score || prevscore[1] + 10 <= ginfo.pinfo[1].score) {
#ifdef DEBUG
			// 状況が変化（situation changed)したことを表すデバッグ表示。相手側にたまっているお邪魔の数も表示する
			if (parameter.turndebugmsg) {
				cerr << "sc! " << ginfo.pinfo[0].ojama[1] << " ";
			}
#endif
			skillmode = false;
		}

		// このターンにお互いにお邪魔を送らなかった場合の、次のターンのたまっているお邪魔の数、フィールド上のお邪魔の数を計算する
		for (int i = 0; i < 2; i++) {
			prevscore[i] = ginfo.pinfo[i].score;
			assumed_droppedojamanum[i] = ginfo.pinfo[i].bb[10].pcount();
			if (ginfo.pinfo[0].ojama[i] >= 10) {
				assumed_ojamanum[i] = ginfo.pinfo[0].ojama[i] - 10;
				assumed_droppedojamanum[i] += 10;
			}
			else {
				assumed_ojamanum[i] = ginfo.pinfo[0].ojama[i];
			}
		}

		// 現在のターンに自分が行えるチェーン数が、現在のターンに相手が行えるチェーン数を上回っている場合、
		// その連鎖を行った結果どのような状況になるかを計算する
		// 一見このターンで行える連鎖数が多いということは有利に見えるが、相手が将来より大きな連鎖でカウンターしてくる可能性がある
		// 相手がこのターンに連鎖を行わなかった場合、将来どの程度の連鎖を相手が行う事ができるかを計算し、
		// 自分の連鎖数より大きい場合は、評価値にペナルティを、小さい場合はボーナスを与えるようにするための処理
		// ただし、以下の条件を満たした場合のみこの処理を行う
		// ・自分が行える連鎖が7連鎖（相手に9以上お邪魔を送り込むことができる）
		// ・残り時間が30秒以上ある場合（処理に時間がかかるため）
		// ・連鎖を行う事で、次のターンに相手に10以上お邪魔を降らすことが可能な場合
		// また、このターンにこちらが連鎖を行わなかった場合に、敵が5ターン以内にこちらが初手で行える連鎖以上の
		// 連鎖を行えない場合は、連座を伸ばしたほうが有利と判断し、特にペナルティやボーナスは与えないことにする
		if (parameter.checkfirstchain && ginfo.myfirstmaxchain > ginfo.enefirstmaxchain && ginfo.myfirstmaxchain >= 7 && pinfo.timeleft >= 30000 && pinfo.ojama[1] - pinfo.ojama[0] + chain_ojamatable[ginfo.myfirstmaxchain] >= 10) {
#ifdef DEBUG
			// デバッグ表示、チェーン数と、次のターンに相手側にたまっているお邪魔の数を表示する
			if (parameter.turndebugmsg) {
				cerr << "d+ " << pinfo.ojama[1] - pinfo.ojama[0] + chain_ojamatable[ginfo.myfirstmaxchain] << " ";
			}
#endif
			// まず、こちらが連鎖を行わない場合に、敵が5ターン以内に発動可能な最大チェーン数をビーム探索で制限時間1秒で計算する
			parameter.beamminx = 0;
			parameter.beammaxx = 8;
			parameter.beamsearchDepth = parameter.enebeamsearchDepth;	// デフォルト5
			parameter.beamsearchWidth = parameter.enebeamsearchWidth;	// デフォルト1600
			parameter.beamsearchChainWidth = parameter.enebeamsearchChainWidth;	// デフォルト 400
			parameter.beamChainmax = -1;
			parameter.beamChainmax2 = -1;
			parameter.firsttimelimit = parameter.enebeamsearchlimit;	// デフォルト 1000
			parameter.stopbeamChainmax = false;
			history[0] = ginfo.pinfo[1];
			// 制限時間の計測開始
			ginfo.t.restart();
			int enechain = beamsearch();
#ifdef DEBUG
			// 敵の通常時の最大チェーン数のデバッグ表示
			if (parameter.turndebugmsg) {
				cerr << "ecn " << enechain << " ";
			}
#endif
			// 敵がこちらが初手で実行可能なチェーン数以上の連鎖を5ターン以内に組めている場合は、
			// このターンで連鎖を行った場合に、敵がどれだけの連鎖を組めるかを計算する
			if (enechain >= ginfo.myfirstmaxchain) {
				// 敵を10ターンビームサーチして最大連鎖数を計算する
				// ビームサーチに関する条件は先ほどと同じ
				parameter.beamsearchDepth = 10;
				history[0] = ginfo.pinfo[1];
				ginfo.t.restart();
				// 1ターン目に chain_ojamatable[ginfo.myfirstmaxchain] だけ相手にお邪魔を降らせた場合でビームサーチを実行する
				enechain = beamsearch(1, chain_ojamatable[ginfo.myfirstmaxchain]);
#ifdef DEBUG
				// その場合の敵の最大チェーン数のデバッグ表示。タイムアウトしていた場合は TO と表示する
				if (parameter.turndebugmsg) {
					cerr << "ec " << enechain << " " << (ginfo.istimeout ? "TO " : "");
				}
#endif

				// 相手が行えるチェーン数が、こちらの最初に行えるチェーン数よりも少ない場合で、
				// タイムアウトしていないか、深さ7以上の探索を行えている場合
				// (相手が途中で死亡するなどの場合で、タイムアウトしていない場合でも深さが　7 以上の探索を行えない場合がある）
				// 初手の連鎖につながる行動を ginfo.firstx, ginfo.firstr に、その際のボーナス評価値として連鎖で降らすことが可能なお邪魔の数を ginfo.firstbonus に設定する
				// 初手の連鎖によるペナルティが発生しないように、maxfirstchainに255を入れておく
				if (enechain < ginfo.myfirstmaxchain && (!ginfo.istimeout || ginfo.beamDepth >= 7)) {
					ginfo.firstx = pinfo.hyouka.chainxr / 10;
					ginfo.firstr = pinfo.hyouka.chainxr % 10;
					ginfo.maxfirstchain = 255;
					// ボーナスは、初手の連鎖で送ることが可能なお邪魔の数 - 相手が可能な最大連鎖によって送られてくるお邪魔の数
					// 確実にこれだけのダメージを与えられるという保証を評価値として加算する
					ginfo.firstbonus = chain_ojamatable[ginfo.myfirstmaxchain] - chain_ojamatable[enechain];
#ifdef DEBUG
					// ボーナスが発生したことを表すデバッグ表示。初手とボーナスを表示する
					if (parameter.turndebugmsg) {
						cerr << "go! x " << ginfo.firstx << " r " << ginfo.firstr << " b " << ginfo.firstbonus << " ";
					}
#endif
				}
				// 相手が可能な連鎖数のほうが、自分の連鎖数より多い場合は、初手で連鎖を行うと不利になるので、ペナルティを与えるようにする
				else if (enechain > ginfo.myfirstmaxchain) {
					// 初手のボーナスが発生しないようにする
					ginfo.firstx = -100;
					// 5連鎖以上の行動を行おうとした場合、ペナルティとして相手が行える連鎖によって降ってくるお邪魔の数を設定する
					ginfo.maxfirstchain = 5;
					ginfo.firstchainpenalty = chain_ojamatable[enechain];
#ifdef DEBUG
					// ペナルティが発生したことを表すデバッグ表示
					if (parameter.turndebugmsg) {
						cerr << "p! x " << pinfo.hyouka.chainxr / 10 << " r " << pinfo.hyouka.chainxr % 10 << " p " << ginfo.firstchainpenalty << " ";
					}
#endif
				}
				// チェーン数が同じ場合は、ボーナスも、ペナルティも発生しないようにする
				else {
					ginfo.firstx = -100;
					ginfo.maxfirstchain = 255;
				}
			}
			// 敵がこちらが初手で実行可能なチェーン数以上の連鎖を5ターン以内に組めていない場合は、
			// ここで発動するより、連鎖を伸ばしたほうが有利になると判断し、ボーナスも、ペナルティも発生しないようにする
			else {
				ginfo.firstx = -100;
				ginfo.maxfirstchain = 255;
			}
		}
		// ボーナスも、ペナルティも発生しないようにする
		else {
			ginfo.firstx = -100;
			ginfo.maxfirstchain = 255;
		}

		// 初手で、大連鎖を組むためのビーム探索を行う。
		// parameter.beamChainmax1 以上の連鎖を可能な限り早く達成した上で、その後より大きな連鎖を組むことができる
		// 行動を計算し、parameter.beamChainmax1 以上の連鎖を達成した深さまでの行動を記録し、その行動を優先して
		// 取らせるようにする（実際に探索した深さよりも浅い場所で行動の記録を打ち切るのは、あまり深くまで行動を
		// 優先してしまうと、早い段階での相手の大連鎖に対抗できなくなってしまうため。大連鎖を組めるような状況を
		// 作っておき、あとは通常の探索による評価を信頼することにする）
		// 具体的には、8ターンまでは、相手が大連鎖を行う事はほぼ不可能なので、ここで見つけた行動を無条件でとることにし
		// それ以降もその行動に対して評価値に大きなボーナスを与えるようにする

		if (ginfo.turn == 0) {
			beamflag = true;
#ifdef DEBUG
			ginfo.aicount = 0;
#endif
			// ビーム探索に関する設定
			// ビーム探索を行う際は、必ず最初の状況を history[0] に入れてから実行する必要がある
			history[0] = ginfo.pinfo[0];
			// ここで設定されたチェーン数を最初に達成した深さにおいて、
			// ここで設定されたチェーン数 + parameter.beamChainmax 未満のチェーン数の行動をすべて削除する
			// 探索した行動が、ここで設定されたチェーン数を可能な限り早く達成する行動に限定されるようにする
			// (大連鎖を組んでも、その途中で大連鎖を発動できなければ、せっかく組んだ大連鎖よりも先に敵が
			//  大連鎖を発動した場合、対抗できない場合があるため）
			parameter.beamChainmax = parameter.beamChainmax1;
			// 上記の連鎖を発見した際にその先の探索を打ち切るかどうか
			parameter.stopbeamChainmax = parameter.stopbeamChainmax;
			// このビーム探索に所要する時間を計測するタイマー
			Timer beamtime;
			// ビーム探索によってみつかった最大チェーン数を記録する
			int firstmaxchain = beamsearch();
			// 最大連鎖数を達成した最小の深さ
			firstbeamdepth = ginfo.beamchainmaxdepth;
			// parameter.beamChainmax 以上の連鎖が見つかっていた場合は、その連鎖が見つかった深さを
			// firstbeamdepth とする
			if (firstmaxchain >= parameter.beamChainmax1) {
				firstbeamdepth = ginfo.eachchaindepth[parameter.beamChainmax1];
			}
			// firstbeamdepth までの行動を firstbestbeamhistory にコピーする
			memcpy(firstbestbeamhistory, bestbeamhistory, sizeof(PlayerInfo) * MAX_BEAM_DEPTH);

#ifdef DEBUG
			// デバッグ表示
			if (parameter.turndebugmsg) {
				cerr << "fbd " << firstbeamdepth << " t " << ginfo.t.time() << endl;
			}
			turndata.setbeam(beamtime.time(), ginfo.aicount);
			if (parameter.debugmsg && parameter.usett) {
				tt.dump_totalcount();
			}
#endif
		}

		// 最初に大連鎖を狙うビーム探索を行っていた場合、8ターンまでに敵がお邪魔を降らせてくる可能性はほぼないので、8以前では無条件でビーム探索の結果得られた行動をとらせる
		if (beamflag && ginfo.turn < firstbeamdepth && ginfo.turn <= 8) {
			ginfo.beamx = firstbestbeamhistory[ginfo.turn + 1].px;
			ginfo.beamr = firstbestbeamhistory[ginfo.turn + 1].pr;
			output(ginfo.beamx, ginfo.beamr);
#ifdef DEBUG
			// デバッグ表示
			if (parameter.turndebugmsg) {
				cerr << " Beam x " << static_cast<int>(ginfo.beamx) << " r " << static_cast<int>(ginfo.beamr) << endl;
			}
			turndata.set(ginfo.turn, 0, 0, ginfo.aicount, 0, 0);
#endif
			continue;
		}

		// 最初のビームサーチの探索を超えた場合は、最初のビームサーチの探索はもう使えないので無効化するために beamflag を flase にする
		if (ginfo.turn >= firstbeamdepth) {
			beamflag = false;
		}

		// 相手がお邪魔を降らせてきた場合のボーナス関連のデータの初期化
		for (int i = 0; i < MAX_DEPTH; i++) {
			ginfo.hyoukabonus[i] = 0;
			ginfo.hyoukabonusx = -100;
		}
		
		// 探索深さの基本設定
		ginfo.enesearchDepth = parameter.enesearchDepth;
		ginfo.mysearchDepth = parameter.searchDepth;

		// 残り思考時間に応じて、探索の深さを補正する
		// 深さ4だと約500ms、3だと約20ms、2だと約5msかかるようなので、余裕を多少もって下記のようにする（この情報はかなり古いので修正する必要がある）
		// 大体の目安
		// 自分: 深さ4 300ms  深さ3 15ms 深さ2 0.5
		// 敵:   深さ4 2500ms 深さ3 30ms 深さ2 0.2
		if (!parameter.ignoretime) {
			if (ginfo.pinfo[0].timeleft < 100 || ginfo.pinfo[0].timeleft < 5 * (MAX_TURN - ginfo.turn)) {
				if (ginfo.mysearchDepth > 2) {
					ginfo.mysearchDepth = 2;
				}
				if (ginfo.enesearchDepth > 2) {
					ginfo.enesearchDepth = 2;
				}
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " dep 2/2 ";
				}
#endif
			}
			else if (ginfo.pinfo[0].timeleft < 10 * (MAX_TURN - ginfo.turn)) {
				if (ginfo.mysearchDepth > 3) {
					ginfo.mysearchDepth = 3;
				}
				if (ginfo.enesearchDepth > 2) {
					ginfo.enesearchDepth = 2;
				}
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " dep 3/2 ";
				}
#endif
			}
			else if (ginfo.pinfo[0].timeleft < 30 * (MAX_TURN - ginfo.turn)) {
				if (ginfo.mysearchDepth > 3) {
					ginfo.mysearchDepth = 3;
				}
				if (ginfo.enesearchDepth > 3) {
					ginfo.enesearchDepth = 3;
				}
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " dep 3/3 ";
				}
#endif
			}
			else if (ginfo.pinfo[0].timeleft < 100 * (MAX_TURN - ginfo.turn)) {
				if (ginfo.mysearchDepth > 4) {
					ginfo.mysearchDepth = 4;
				}
				if (ginfo.enesearchDepth > 3) {
					ginfo.enesearchDepth = 3;
				}
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " dep 4/3 ";
				}
#endif
			}
		}
		// 先に敵の思考を行う
		// 敵がスキルを使うか、使わないか、敵のフィールドにいつお邪魔が落下するかなど、様々な状況に対して
		// 敵が行える敵にとって最も有利な状況を計算して enemystatusdata に記録する。
		// その情報を、自身の思考時に利用する
		// 時間の計測開始
#ifdef DEBUG
		ginfo.aicount = 0;
#endif
		t.restart();
		// 敵の思考であることを表すフラグを立てる
		ginfo.isenemy = true;
		// 探索深さの設定
		ginfo.searchDepth = ginfo.enesearchDepth;
		// 各深さにおける敵の状況を表す estatusdata の初期化
		enemystatusdata.clear(ginfo.searchDepth);
		// 深さ0の状況に敵の初期状況をコピーする
		history[0] = ginfo.pinfo[1];
		// 置換表の初期化
		if (parameter.usett) {
			tt.clear();
		}
		// 探索開始
		ai();
		// 敵が行える各チェーン数に対して、そのチェーンを行える最小の深さを表すテーブルの初期化（1000を入れておく）
		for (int i = 0; i <= MAX_CHAIN; i++) {
			ginfo.eneeachchaindepth[i] = 1000;
		}
		ginfo.eneeachchaindepth[ginfo.enefirstmaxchain] = 0;
		ginfo.enemaxchain = ginfo.enefirstmaxchain;

		// 初期状態で敵のフィールドにいつお邪魔が降ってくるかを表すビットの計算
		//int enedepthbit = (1 << (ginfo.pinfo[0].ojama[1] / 10)) - 1;
		// スキルを使用しない場合（estatus[0])とした場合(estatus[1])の良いほうを計算し、estatus[2]に設定する
		// 敵のフィールドにお邪魔が降ってくるビットパターンは 2 ^ ginfo.searchDepth 種類ある。そのすべてに対して計算を行う
		for (int j = 0; j < (1 << ginfo.searchDepth); j++) {
			// 各深さに対して計算を行う
			for (int k = 1; k <= ginfo.searchDepth; k++) {
				// enemystatusdata.data[3] は enemystatusdata.data[1] と enemystatusdata.data[2] のうち、敵にとって良いほうの値を採用する
				enemystatusdata.data[3][j][k].ischain = enemystatusdata.data[1][j][k].ischain | enemystatusdata.data[2][j][k].ischain;
				enemystatusdata.data[3][j][k].isdead = enemystatusdata.data[1][j][k].isdead & enemystatusdata.data[2][j][k].isdead;
				enemystatusdata.data[3][j][k].maxchain = max(enemystatusdata.data[1][j][k].maxchain, enemystatusdata.data[2][j][k].maxchain);
				enemystatusdata.data[3][j][k].maxojama = max(enemystatusdata.data[1][j][k].maxojama, enemystatusdata.data[2][j][k].maxojama);
				enemystatusdata.data[3][j][k].maxskillminus = max(enemystatusdata.data[1][j][k].maxskillminus, enemystatusdata.data[2][j][k].maxskillminus);
				enemystatusdata.data[3][j][k].skilldeleteblocknum = max(enemystatusdata.data[1][j][k].skilldeleteblocknum, enemystatusdata.data[2][j][k].skilldeleteblocknum);
				enemystatusdata.data[3][j][k].minojamanum = min(enemystatusdata.data[1][j][k].minojamanum, enemystatusdata.data[2][j][k].minojamanum);
				enemystatusdata.data[3][j][k].maxskillojama = max(enemystatusdata.data[1][j][k].maxskillojama, enemystatusdata.data[2][j][k].maxskillojama);
				// enemystatusdata.data[4] は enemystatusdata.data[0] と enemystatusdata.data[1] と enemystatusdata.data[2] のうち、敵にとって良いほうの値を採用する
				enemystatusdata.data[4][j][k].ischain = enemystatusdata.data[0][j][k].ischain | enemystatusdata.data[1][j][k].ischain | enemystatusdata.data[2][j][k].ischain;
				enemystatusdata.data[4][j][k].isdead = enemystatusdata.data[0][j][k].isdead & enemystatusdata.data[1][j][k].isdead & enemystatusdata.data[2][j][k].isdead;
				enemystatusdata.data[4][j][k].maxchain = max(enemystatusdata.data[0][j][k].maxchain, enemystatusdata.data[3][j][k].maxchain);
				enemystatusdata.data[4][j][k].maxojama = max(enemystatusdata.data[0][j][k].maxojama, enemystatusdata.data[3][j][k].maxojama);
				enemystatusdata.data[4][j][k].maxskillminus = max(enemystatusdata.data[0][j][k].maxskillminus, enemystatusdata.data[3][j][k].maxskillminus);
				enemystatusdata.data[4][j][k].skilldeleteblocknum = max(enemystatusdata.data[0][j][k].skilldeleteblocknum, enemystatusdata.data[3][j][k].skilldeleteblocknum);
				enemystatusdata.data[4][j][k].minojamanum = min(enemystatusdata.data[0][j][k].minojamanum, enemystatusdata.data[3][j][k].minojamanum);
				enemystatusdata.data[4][j][k].maxskillojama = max(enemystatusdata.data[0][j][k].maxskillojama, enemystatusdata.data[3][j][k].maxskillojama);
				// インデックスが3と4の minblocknum は使わないので計算しない
			}
		}
		int etime = t.time();
#ifdef DEBUG
		// 探索深さのデバッグ表示
		if (parameter.turndebugmsg) {
			cerr << " e " << t.time() << " d " << ginfo.enesearchDepth << "/" << parameter.enesearchDepth << " ";
		}
#endif

		// 最初の大連鎖を狙うビーム探索の結果を採用できない状況の場合
		if (beamflag == false) {
			// 残り時間が30秒以上で、連鎖を組むためのビーム探索を行う場合
			if (parameter.nextchain && ginfo.pinfo[0].timeleft >= 30000) {
				// ビーム探索のタイマーのリセット
				ginfo.t.restart();
				// 自分の状況を history[0] にセット
				history[0] = ginfo.pinfo[0];
				Timer beamtime;
				// ビーム探索の設定
				parameter.beamminx = 0;
				parameter.beammaxx = 8;
				parameter.beamsearchDepth = parameter.nextbeamsearchDepth;
				parameter.beamsearchWidth = parameter.nextbeamsearchWidth;
				parameter.beamsearchChainWidth = parameter.nextbeamsearchChainWidth;
				parameter.beamChainmax2 = -1;
				parameter.stopbeamChainmax = false;
				parameter.firsttimelimit = parameter.nextfirsttimelimit;
				int beamchainnum;
				// 連鎖を目指している場合
				// 最初の行動で行える連鎖数が7以下の場合は、10連鎖を最速で狙う
				// それ以外の場合は特に制限を加えず大連鎖を探索する
				if (skillmode == false) {
					if (ginfo.myfirstmaxchain <= 7) {
						parameter.beamChainmax = 10;
					}
					else {
						parameter.beamChainmax = -1;
					}

					// ビーム探索を行う
					beamchainnum = beamsearch();
					// 最も評価値の高い初手の行動を ginfo.beamx, beamr に記録する
					ginfo.beamx = bestbeamhistory[1].px;
					ginfo.beamr = bestbeamhistory[1].pr;

					// 探索で得られた、各チェイン数を達成可能な最小の深さを計算する
					// 敵と味方に対する初期化
					for (int i = 0; i <= MAX_CHAIN; i++) {
						ginfo.myeachchaindepth[i] = 1000;
						ginfo.eneeachchaindepth[i] = 1000;
					}
					
					// 初手で実行可能な最大チェーンの深さを0とする
					ginfo.eachchaindepth[ginfo.myfirstmaxchain] = 0;
					// 最大チェーン数を計算する
					ginfo.mymaxchain = max(ginfo.myfirstmaxchain, beamchainnum);
					// 記録されていないチェーン数の間を埋める計算を行う
					// x連鎖ができる場合、y<x を満たす y連鎖も同じ深さで行えるということにする
					for (int i = ginfo.mymaxchain; i >= 0; i--) {
						for (int j = 0; j < i; j++) {
							if (ginfo.eachchaindepth[i] < ginfo.eachchaindepth[j]) {
								ginfo.eachchaindepth[j] = ginfo.eachchaindepth[i];
							}
						}
						// ビーム探索では、ginfo.eachchaindepth に結果が格納されるので、それを
						// ginfo.myeachchaindepth にコピーする
						ginfo.myeachchaindepth[i] = ginfo.eachchaindepth[i];
					}
#ifdef DEBUG
					// 結果のデバッグ表示
					if (parameter.turndebugmsg) {
						cerr << "mych " << ginfo.mymaxchain << " ";
						int mind = 100;
						for (int i = ginfo.mymaxchain; i >= 0; i--) {
							if (mind > ginfo.myeachchaindepth[i]) {
								cerr << i << ":" << ginfo.myeachchaindepth[i] << " ";
								mind = ginfo.myeachchaindepth[i];
							}
						}
					}
#endif					
					// 敵のビーム探索も行う。ただし、時間は短めに（0.5秒）
					parameter.beamsearchWidth = parameter.nextenebeamsearchWidth; // デフォルト750
					parameter.beamsearchChainWidth = parameter.nextenebeamsearchChainWidth; // デフォルト250
					parameter.beamChainmax2 = -1;
					parameter.stopbeamChainmax = false;
					parameter.firsttimelimit = parameter.nextenebeamsearchlimit; // デフォルト 500;

					ginfo.t.restart();
					history[0] = ginfo.pinfo[1];

					// 初手で5連鎖できない場合は、10連鎖を、そうでなければ初手の連鎖数+5を目指すようにする
					if (ginfo.enefirstmaxchain <= 5) {
						parameter.beamChainmax = 10;
					}
					else {
						parameter.beamChainmax = ginfo.enefirstmaxchain + 5;
					}
					beamchainnum = beamsearch();

					// 自分の場合と同様に、各チェーン数を達成可能な最小の深さを計算する
					ginfo.eachchaindepth[ginfo.enefirstmaxchain] = 0;
					ginfo.enemaxchain = max(ginfo.enefirstmaxchain, beamchainnum);
					for (int i = ginfo.enemaxchain; i >= 0; i--) {
						for (int j = 0; j < i; j++) {
							if (ginfo.eachchaindepth[i] < ginfo.eachchaindepth[j]) {
								ginfo.eachchaindepth[j] = ginfo.eachchaindepth[i];
							}
						}
						ginfo.eneeachchaindepth[i] = ginfo.eachchaindepth[i];
					}
#ifdef DEBUG
					// 結果のデバッグ表示
					if (parameter.turndebugmsg) {
						cerr << "ench " << ginfo.enemaxchain << " ";
						int mind = 100;
						for (int i = ginfo.enemaxchain; i >= 0; i--) {
							if (mind > ginfo.eneeachchaindepth[i]) {
								cerr << i << ":" << ginfo.eneeachchaindepth[i] << " ";
								mind = ginfo.eneeachchaindepth[i];
							}
						}
					}
#endif

					// ここまでで得られた情報を元に、大連鎖を狙うか、スキル発動を狙うかを決定する
					// 以下の場合にスキル発動を狙う
					// 自分が10ターン以内にできる連鎖数が7未満の時
					// そうでなければ下記のすべての条件を満たすとき
					// 　自分のフィールド内にお邪魔が20以上たまっている場合
					// 　敵の落下予定のお邪魔が20未満の場合（20以上たまっている場合は、こちらが大連鎖を行った直後。この条件がなければ、敵がこの時大連鎖を組める状況だと、スキルを狙うようになてしまう）
					//   敵のビーム探索で敵が12連鎖以上を組める場合
					//   自分のビーム探索で達成できる連鎖数が parameter.nextbeamsearchminChain 連鎖（デフォルト値9）未満か、スキルで消せるブロック数が10以上の場合
					//   自分の12連鎖、11連鎖が相手の12連鎖よりも3ターン以上遅い
					//   自分の10連鎖が、相手の12連鎖よりも2ターン以上遅い
					//   自分の9連鎖が、相手の12連鎖よりも1ターン以上遅い
					//   自分の8連鎖が、相手の12連鎖と同じかそれ以上遅い
					// (あまり簡単にスキル発動を目指すようにするとまずい場合が多いので厳しめ設定している）
					if (ginfo.mymaxchain < 7 || 
						(ginfo.pinfo[0].bb[10].pcount() >= 20 && ginfo.pinfo[0].ojama[1] < 20 && ginfo.enemaxchain >= 12 && !(ginfo.mymaxchain >= parameter.nextbeamsearchminChain && pinfo.calcskilldeleteblocknum() < 10) &&
						 (ginfo.myeachchaindepth[12] >= ginfo.eneeachchaindepth[12] + 3 && 
						  ginfo.myeachchaindepth[11] >= ginfo.eneeachchaindepth[12] + 3 &&
						  ginfo.myeachchaindepth[10] >= ginfo.eneeachchaindepth[12] + 2 &&
						  ginfo.myeachchaindepth[9] >= ginfo.eneeachchaindepth[12] + 1 &&
						  ginfo.myeachchaindepth[8] >= ginfo.eneeachchaindepth[12]))) {
#ifdef DEBUG
						if (parameter.turndebugmsg) {
							cerr << "SKILLMODE! ";
						}
#endif
						skillmode = true;
					}
					// そうでなければ、連鎖を目指すことにする
					else {
#ifdef DEBUG
						if (parameter.turndebugmsg) {
							cerr << " bc ";
						}
#endif
					}

					// 連座を目指す場合、連鎖を目指すビーム探索の行動の有効性を検証する
					// 具体的には、ビーム探索の行動をとった場合、その後敵がこちらの探索深さ内で連鎖を起こした場合、
					// カウンターできるかどうかを調べ、カウンターできる場合は、評価値にボーナスを与えるようにする
					if (skillmode == false) {
						// こちらがお邪魔を落とさない場合に、敵が大連鎖を落としてきた場合の、こちらの最大連鎖数を計算する
						// なお、敵が大連鎖を行うまでの間は、敵はお邪魔を落としてこないと仮定する（落としてくると大連鎖の達成が困難であるため）
						// 敵にお邪魔が落ちるビット
						int ebit = 0;
						// 自分と敵にたまっているお邪魔の数
						int myojama = ginfo.pinfo[0].ojama[0];
						int eneojama = ginfo.pinfo[0].ojama[1];
#ifdef DEBUG
						if (parameter.turndebugmsg) {
							cerr << endl;
						}
#endif
						// 自分が行う各探索深さにおいて、チェックする
						for (int d = 1; d <= ginfo.searchDepth; d++) {
							// その深さまでの間で敵のフィールドへのお邪魔の落下を表すビット列の計算
							// 敵味方について、お邪魔が落下する場合は、お邪魔の数も減らす
							if (eneojama >= 10) {
								ebit = ebit + (1 << (d - 1));
								eneojama -= 10;
							}
							if (myojama >= 10) {
								myojama -= 10;
							}
							// その深さで落とされる可能性のあるお邪魔の数
							// その深さで敵が実行可能な最大チェーン数によるお邪魔の数と、
							// その深さで敵がスキルを発動した場合のお邪魔の数の大きいほう
							int onum = chain_ojamatable[enemystatusdata.data[4][ebit][d].maxchain];
							if (onum < enemystatusdata.data[4][ebit][d].maxskillojama) {
								onum = enemystatusdata.data[4][ebit][d].maxskillojama;
							}
							// お邪魔が降ってきた結果、自身にたまるお邪魔の数
							int onum2 = onum + myojama - eneojama;
#ifdef DEBUG
							if (parameter.turndebugmsg) {
								cerr << "onum " << d << " " << ebit << " " << onum << " " << onum2 << endl;
							}
#endif
							// 落とされる可能性のあるお邪魔が10以上で、降ってくる可能性のあるお邪魔が10以上の場合のみチェックする
							if (onum >= 10 && onum2 >= 10) {
								// 深さ7でチェーン数の制限を設けず、500msで探索する
								parameter.beamminx = 0;
								parameter.beammaxx = 8;
								parameter.beamsearchDepth = parameter.droppedbeamsearchDepth; // デフォルト 7
								parameter.beamsearchWidth = parameter.droppedbeamsearchWidth; // デフォルト 350;
								parameter.beamsearchChainWidth = parameter.droppedbeamsearchChainWidth; // デフォルト 100;
								parameter.beamChainmax = -1;
								parameter.beamChainmax2 = -1;
								parameter.firsttimelimit = parameter.droppedbeamsearchlimit; // デフォルト 500;
								parameter.stopbeamChainmax = false;

								history[0] = ginfo.pinfo[0];
								ginfo.t.restart();
								// 初手が ginfo.beamx, ginfo.beamr の行動のみを探索することを表すフラグを立てる
								ginfo.checkonlybeamxr = true;
								// 深さ d で敵が onum このお邪魔を降らせた場合のビーム探索を行う
								int mychain = beamsearch(d, onum);

								// 初手が ginfo.beamx, ginfo.beamr の行動のみを探索することを表すフラグを落としておく
								ginfo.checkonlybeamxr = false;
								dtime += ginfo.t.time();
								// 初手が ginfo.beamx, ginfo.beamr で 深さ d で敵が大連鎖してきた場合にこちらが達成可能な連鎖によるお邪魔の落下数をボーナスとして記録する
								ginfo.hyoukabonusx = ginfo.beamx;
								ginfo.hyoukabonusr = ginfo.beamr;
								ginfo.hyoukabonus[d] = chain_ojamatable[mychain];
#ifdef DEBUG
								if (parameter.turndebugmsg) {
									cerr << "bonus " << mychain << " x " << ginfo.hyoukabonusx << "," << ginfo.hyoukabonusr << " " << ginfo.hyoukabonus[d] << endl;
								}
#endif
							}
						}
						// お互いにsearchDepthまでお邪魔を降らせなかった場合も計算したいが、
						// searchDepth 以降の連鎖が不利であっても、連鎖のタイミングによっては有利になる場合があり、
						// それも綱領に入れるようなうまい方法が思いつかないので何もしない
					}
				}
				// skillmode が true に変更される場合があるので、else if とはしない
				if (skillmode == true) {
					// 500ms でスキル発動を目指すためのビーム探索を行う
					// 特に、ボーナスやペナルティは設けない
					parameter.beamsearchDepth = parameter.skillbeamsearchDepth; // デフォルト 10;
					parameter.beamsearchWidth = parameter.skillbeamsearchWidth; // デフォルト 1000;
					parameter.beamsearchChainWidth = parameter.skillbeamsearchChainWidth; // デフォルト 200;
					parameter.beamChainmax = -1;
					parameter.beamChainmax2 = -1;
					parameter.firsttimelimit = parameter.skillbeamsearchlimit; // デフォルト 500;
					history[0] = ginfo.pinfo[0];
					ginfo.t.restart();
#ifdef DEBUG
					if (parameter.turndebugmsg) {
						cerr << " bs ";
					}
#endif
					beamsearch(0, 0, true);
					ginfo.beamx = bestbeamhistory[1].px;
					ginfo.beamr = bestbeamhistory[1].pr;
				}
			}
			// 時間切れなどの場合は、beamx に -100 を代入して無効化する
			else {
				ginfo.beamx = -100;
			}
		}

#ifdef DEBUG
		// どちらを目指しているかのデバッグ表示
		if (parameter.turndebugmsg) {
			if (skillmode) {
				cerr << "smode ";
			}
			else {
				cerr << "cmode ";
			}
		}
#endif

		// 特定のターンの特定の行動の評価値を知るための処理（デバッグ用）
		// parameter.beamx, parameter.beamr の値を ginfo.beamx, ginfo.beamr に設定することで、その評価値をこののちに表示できるようにする
		if (parameter.checkturn >= 0 && ginfo.turn == parameter.checkturn && parameter.beamx >= -1) {
			ginfo.beamx = parameter.beamx;
			ginfo.beamr = parameter.beamr;
		}
		
		// 味方の探索開始
#ifdef DEBUG
		ginfo.aicount = 0;
#endif
		// 計測時間のリセット
		t.restart();
		// 味方の探索フラグの設定
		ginfo.isenemy = false;
		// 深さ最大からはじめ、besthyouka が見つからなかった場合（100%負けになる場合）は、深さを1ずつ減らして探索しなおすことで最大限あがく
		// そうしないと、最大深さて先で生き残る手が見つからない場合は、あきらめてしまうことになる
		for (ginfo.searchDepth = ginfo.mysearchDepth ; ginfo.searchDepth >= 1; ginfo.searchDepth--) {
			// 最大評価値の初期化（－無限大にしておく）
			besthyouka.total = -INF;
			bestbeamhyouka.total = -INF;
			// 深さ0の状況に味方の初期状態をコピーする
			history[0] = ginfo.pinfo[0];
			// 置換表の初期化
			if (parameter.usett) {
				tt.clear();
			}
			// 探索を行う
			ai();
			// 最大評価値が見つかった場合はループを抜ける
			if (besthyouka.total != -INF) {
				break;
			}
		}
#ifdef DEBUG
		if (parameter.turndebugmsg) {
			cerr << " m " << t.time() << " d " << ginfo.searchDepth << "/" << ginfo.mysearchDepth << "/" << parameter.searchDepth << " ";
		}

		// デバッグ表示
		if (parameter.debugmsg) {
			// 最初の敵の手番で敵の盤面にお邪魔が降ってきたかどうかを表すフラグの計算
			bool isenefirstojamadropped = false;
			if (besthistory[0].eneojamadroppeddepthbit & 1) {
				isenefirstojamadropped = true;
			}
			// 敵の各状況における行動の結果のデバッグ表示
			for (int i = 1; i <= ginfo.enesearchDepth; i++) {
				cerr << "depth " << i << endl;
				for (int j = 0; j < (1 << i); j++) {
					if ((isenefirstojamadropped && (j & 1) == 0) || (!isenefirstojamadropped && (j & 1) == 1)) {
						continue;
					}
					for (int k = 0; k < ESTATUS_NUM; k++) {
						cerr << "ene " << k << " ";
						for (int l = 0; l < ginfo.enesearchDepth; l++) {
							if (l >= i) {
								cerr << " ";
							}
							else {
								if (j & (1 << l)) {
									cerr << "1";
								}
								else {
									cerr << "0";
								}
							}
						}
						cerr << " ";
						enemystatusdata.data[k][j][i].dump();
					}
				}
				cerr << endl;
			}
			// 最善手で敵が死亡している場合は、死亡している深さまでの味方の盤面の状況を表示
			if (besthyouka.enedeaddepth > 0) {
				cerr << "normal" << endl;
				dump_fields(besthistory, besthyouka.enedeaddepth);
				cerr << "beam" << endl;
				dump_fields(bestbeamhistory_log, besthyouka.enedeaddepth);
			}
			// そうでなければ探索深さまで表示
			else {
				cerr << "normal" << endl;
				dump_fields(besthistory, ginfo.mysearchDepth);
				cerr << "beam" << endl;
				dump_fields(bestbeamhistory_log, ginfo.mysearchDepth);
			}
			if (parameter.usett) {
				tt.dump_count();
			}
		}


		if (parameter.debugmsg) {
			cerr << "beam flag " << beamflag << " depth " << ginfo.beamDepth << " turn " << ginfo.turn << endl;
		}
#endif
		// 最初のビーム探索が有効な場合
		if (beamflag && ginfo.turn < firstbeamdepth) {
			// このターンにお邪魔が降ってくる場合は、最初のビーム探索の結果が無意味になるため、beamflag を false にして通常の行動をとらせる
			if (ginfo.pinfo[0].ojama[0] >= 10) {
#ifdef DEBUG
				if (parameter.debugmsg) {
					cerr << " Ojama dropped. Beam canceled." << endl;
				}
				else if (parameter.turndebugmsg) {
					cerr << " Ojama dropped. ";
				}
#endif
				beamflag = false;
			}
			// 以下の状況ではあらかじめ計算しておいた行動を破棄する
			// ・通常の評価値とビーム探索の行動の評価値が等しい
			// または、以下のすべての条件を満たす
			// ・「通常の評価値が parameter.beamIgnoreHyoukamax を超え、ビーム探索の行動の評価が parameter.beamIgnoreHyoukamax を下回る」でない
			// ・ビーム探索の行動の評価値が -INF （死亡する）でない
			// ・「ビーム探索の行動の評価値が parameter.beamIgnoreHyoukamin 以下で、通常の評価値とビーム探索の評価値の差が parameter.beamIgnoreHyoukadiff 以上」でない
			else if ((besthyouka.total == bestbeamhyouka.total) || 
				     (!(besthyouka.total >= parameter.beamIgnoreHyoukamax && bestbeamhyouka.total < parameter.beamIgnoreHyoukamax) && 
						bestbeamhyouka.total != -INF && 
						!(bestbeamhyouka.total < parameter.beamIgnoreHyoukamin && besthyouka.total - bestbeamhyouka.total >= parameter.beamIgnoreHyoukadiff))) {
				output(ginfo.beamx, ginfo.beamr);
				// 味方の思考時間を計測する
				int mtime = t.time();
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " Beam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.total << " bh " << bestbeamhyouka.total << endl;
				}
				turndata.set(ginfo.turn, mtime, etime, ginfo.aicount, ginfo.searchDepth, ginfo.enesearchDepth);
#endif
				continue;
			}
			// それ以外の場合はあらかじめ計算しておいた行動は捨てて、通常の行動をとる
			else {
				// ただし、ビーム探索の行動と、通常の行動が一致しない場合のみ beamflag を false にする
				if (ginfo.beamx != besthistory[0].x || ginfo.beamr != besthistory[0].r) {
					beamflag = false;
				}
#ifdef DEBUG
				if (beamflag == false && parameter.turndebugmsg) {
					cerr << " Canceled Beam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.total << " bh " << bestbeamhyouka.total << endl;
				}
#endif
			}
		}
		// 初手でない、ビーム探索の結果を採用するかどうか
		else if (beamflag == false && parameter.nextchain) {
			// 通常の評価値と、ビーム探索の行動の評価値の差が parameter.nextbeamIgnoreHyoukadiff 以下の場合は、ビーム探索の行動を優先する
			if (bestbeamhyouka.total != -INF && !(besthyouka.total - bestbeamhyouka.total >= parameter.nextbeamIgnoreHyoukadiff)) {
				output(ginfo.beamx, ginfo.beamr);
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " NBeam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.total << " bh " << bestbeamhyouka.total << endl; // "," << bestbeamhyouka.eneskillojama << endl;

				}
#endif
				// 味方の思考時間を計測する
				int mtime = t.time();
				// 敵と味方の思考時間を記録する
#ifdef DEBUG
				turndata.set(ginfo.turn, mtime, etime, ginfo.aicount, ginfo.searchDepth, ginfo.enesearchDepth);
#endif
				continue;
			}
			// そうでなければ、ビーム探索の行動を取らない
			else {
#ifdef DEBUG
				if (parameter.turndebugmsg) {
					cerr << " NBC x " << ginfo.beamx << " r " << ginfo.beamr << " bh " << bestbeamhyouka.total << " "; // "," << bestbeamhyouka.eneskillojama << " ";// << bestbeamhyouka.normalblocknum << " ";
				}
#endif
			}
		}

		// 最善手を取り出す（besthistory[0]に記録されている）
		int bestx, bestr;
		bestx = besthistory[0].x;
		bestr = besthistory[0].r;
		// 最善手の出力
		output(bestx, bestr);
		// 計測時間、最善手を見つけた際の探索深さの表示
#ifdef DEBUG
		if (parameter.turndebugmsg) {
			cerr << "Act " << bestx << " " << bestr << " h " << besthyouka.total << " ";
		}
		if (parameter.debugmsg) {
			cerr << " aic " << ginfo.aicount << " ";
		}
		if (parameter.turndebugmsg) {
			cerr << endl;
		}
		// 味方の思考時間を計測する
		int mtime = t.time();
		// 敵と味方の思考時間を記録する
		turndata.set(ginfo.turn, mtime, etime, ginfo.aicount, ginfo.searchDepth, ginfo.enesearchDepth);
#endif
		lastturn = ginfo.turn;
	}
	// ゲームオーバー後の表示
	// 実際のゲームでは、ゲームオーバー時にプロセスが強制終了するのでここには来ないはず
	// コマンドプロンプトなどから実行した場合に表示される
	// 全所要時間をデバッグ表示
	cerr << "Total time " << totaltime.time() << " dtime " << dtime <<  endl;
	// 各ターンのデータをデバッグ表示
	turndata.dump(lastturn);
	if (parameter.usett) {
		tt.dump_totalcount();
	}
	cerr.flush();
    return 0;
}

