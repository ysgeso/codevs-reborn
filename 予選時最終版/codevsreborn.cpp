#include "codevsreborn.hpp"
#include "tt.hpp"


mt19937 mt(20190405);
uniform_real_distribution<> rnd_01(0, 1.0);

GameInfo ginfo;

// AIの思考に関するパラメータ
Parameter parameter;

int errornum = 0;

TT_TABLE beamtt;

std::mutex mtx;

int maxfirstchain;
double chainpenalty;
int firstx, firstr;
double firstbonus;

// AIの行動の結果を出力する
void output(int bestx, int bestr) {
	// AIの行動の出力
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
// （besthistory を並べて表示するための関数。引数が depth 担っているのはそのため）
// PlayerInfo の dump_field の複数版
// 引数：
//   pinfos: PlayerInfoの配列のポインタ
//   depth: pinfos の配列の数 - 1
void dump_fields(PlayerInfo *pinfos, int depth, bool isprev=false) {
	int field[MAX_BEAM_DEPTH + 1][BOARD_WIDTH][BOARD_HEIGHT];
	//cerr << "a" << endl;
	for (int i = 0; i <= depth; i++) {
		//cerr << "b " << i << endl;
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
		cerr << "oj " << setw(3) << pinfos[i].getojama << "/" << setw(3) << pinfos[i].minojamanum << spaces;
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
	if (parameter.aitype == AITYPE::BEAM_RENSA) {
		for (int i = 0; i <= depth; i++) {
			cerr << "bc " << setw(2) << pinfos[i].hyouka.chain << " n" << setw(3) << static_cast<int>(pinfos[i].hyouka.normalblocknum) << spaces;
		}
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
				x = pinfos[i].hyouka.normalblocknum / 10;
				r = pinfos[i].hyouka.normalblocknum % 10;
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
	// 最後に評価値を表示する
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
#ifdef USE_ZORBIST
	// zorbist ハッシュの生成
	// 乱数関連
	// 乱数の種は固定しておく
	mt19937 mt(20190405);
	uniform_int_distribution<int> rnd16(0, (1 << 16) - 1);
	for (int i = 1; i <= 10; i++) {
		for (int y = 0; y < LOAD_BOARD_HEIGHT ; y++) {
			for (int x = 0; x < BOARD_WIDTH ; x++) {
				zorbisthashtable[i][x][y] = create_hash(mt, rnd16);
			}
		}
	}
	memset(zorbisthash_x_table, 0, sizeof(uint64_t) * 11 * BOARD_WIDTH * (1 << LOAD_BOARD_HEIGHT));
	for (int i = 0; i < (1 << LOAD_BOARD_HEIGHT); i++) {
		for (int y = 0; y < LOAD_BOARD_HEIGHT; y++) {
			if (i & (1 << y)) {
				for (int j = 1; j <= 10; j++) {
					for (int x = 0; x < BOARD_WIDTH; x++) {
						zorbisthash_x_table[j][x][i] ^= zorbisthashtable[j][x][y];
					}
				}
			}
		}
	}
#endif

	// 所要時間を表示
	if (parameter.turndebugmsg) {
		cerr << t.time() << " ms" << endl;
	}
	return t.time();
}

// 探索で見つかった最高評価値
HyoukaData besthyouka;
// ビームサーチで見つかった行動をとった場合の最高評価値
HyoukaData bestbeamhyouka;
// 引数が表す番号のブロックが動いたか（次に消滅判定が必要かどうか）、消滅したか（周囲のブロックの範囲の再計算が必要）どうかを表す。初期化処理を省くため、
// blockmovecount と等しい場合に動いたことを表す。
// ブロックの消滅チェックに使うので、そのブロックの対（10-そのブロックの番号）のブロックも同時に動いたことにする
// blockmovecount + 1 と等しい場合消滅したことを表す。
// 毎ターンの最初にすべて -1 に、blockmovecount を 0 にすることで初期化する。
// blockmoved の値が用済みになった時点で blockmovecount に 2 を加算する
int blockmoved[11];
int blockmovecount;
// 関数 ai() を呼び出した回数（デバッグ用）
int aicount;
// 敵の思考かどうか
bool isenemy;

// 関数 ai の探索時の、各深さのプレーヤー情報を格納する配列変数
PlayerInfo history[MAX_DEPTH + 1];
// 最高の評価値の各深さの情報を格納する配列変数。思考の可視化のために記録するが、コピーが重いので、
// 最善手のみ記録するバージョンも作る（todo）
PlayerInfo besthistory[MAX_DEPTH + 1];
PlayerInfo bestbeamhistory_log[MAX_DEPTH + 1];

EnemyStatusData enemystatusdata;

bool action(PlayerInfo& pinfoorig, PlayerInfo& pinfo, bool changehash) {
	BitBoard deletebb;

	int x = pinfoorig.x;
	int r = pinfoorig.r;

	uint8_t blockmoved[11];
	uint8_t blockmovecount = 1;

	memset(blockmoved, 0, 11);
//	for (int i = 0; i < 11; i++) {
//		blockmoved[i] = 0;
//	}
	// スキル使用行動の場合
	if (x == -1) {
		// r == 0で、スキルポイントが80以上あり、5のブロックが存在する場合のみスキルを使用する
		// そうでなければ false を返す
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

	// それ以外の行動の場合
	if (x != -1) {
		// パック情報を盤に置く
		// パックの情報を表す 2x2 の配列のループ
		for (int px = 0; px < 2; px++) {
			// パックを落下する列のブロック数を表す変数への参照
			uint8_t& blocknum = pinfo.blocknum[x + px];
			for (int py = 0; py < 2; py++) {
				// 落下するブロックの種類
				int block = ginfo.blockinfo[pinfo.turn][r][px][py];
				if (block > 0) {
					// 任意のブロックを表す x + px 列にブロックを追加
					pinfo.setbb(0, x + px, blocknum, changehash);
					// block のブロックを表す x + px 列にブロックを追加
					pinfo.setbb(block, x + px, blocknum, changehash);
					// block の周囲8マスのブロックを表すBitBoard にブロックが落下した (x + px, blocbnum) の
					// 周囲8マスのブロックを追加する
					pinfo.bb[block + 10] |= around_bb_table[x + px][blocknum];
					// block のブロックが移動したことを記録する
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
		if (parameter.showchain) {
			cerr << "chain " << static_cast<int>(chain) << endl;
			pinfo.dump_field();
		}
		// このループでブロックが消去されていないことにする
		deleted = false;
		// スキルを使用した場合で、スキルの消去処理を行っていない場合
		if (x == -1 && !pinfo.skillusedinthisdepth) {
			// スキルポイントを 0 にする
			pinfo.skill[0] = 0;
			// 消去されたブロックを計算する。
			// A: 5のブロックと、5の周囲のブロックの和集合が消去される可能性のあるブロック
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
				// 動いたブロックとその対になるブロックだけチェックすればよい
				// 補足：blockmoved[i] と blockmoved[10-i] は必ずチェック対象となるようにしてあるはず（そうなってなければバグ）
				if (blockmoved[i] == blockmovecount) {
					// i 番のブロックと、10 - i 番目のブロックの周囲8マスのブロックの Bitmap の積集合が、消滅するブロック
					deletebb |= pinfo.bb[i] & pinfo.bb[20 - i];
				}
			}
		}
		// 今回のブロック消滅判定は終了したので、blockmoved の値をすべてクリアするため、blockmovecount を 2 増やす
		blockmovecount += 2;

		// 消滅したブロックがある場合の処理
		if (!deletebb.iszero()) {
			//if (pinfoorig.turn == 30 && x == 2 && r == 2) {
			//	cerr << "chain " << chain << endl;
			//	pinfo.dump_field();
			//	deletebb.dump();
			//}
			// このループでブロックが消滅したことを表す変数のフラグを立てる
			deleted = true;
			// チェイン数を 1 増やす
			chain++;
			// 各行についてブロックを削除する
			for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
				// x2 行目に消滅するブロックが存在する場合
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
								pinfo.setbbx(i, x2, newbbx, changehash);
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
								// ブロックが削除されていて（newbbx が 0）、連鎖の対象となっていなければ、周囲8マスの計算をし直すようにする
								else if (blockmoved[i] != blockmovecount) {
									blockmoved[i] = blockmovecount + 1;
								}
							}
						}
						// 任意のブロックが存在しない場合は残りも存在しないはずなので抜ける
						else if (i == 0) {
							break;
						}
					}
				}
			}
			// 各ブロック（1～9）の周囲8マスのブロックの再計算
			// 消滅したブロックの周囲の8マスのビットをOFFにするという方法では、OFFにしたビットの周囲に消滅したブロックと同じ
			// 番号のブロックが存在した場合はうまくいかないので、いちから計算しなおす
			for (int i = 1; i <= 9; i++) {
				// 移動、削除されたブロックのみ再計算すればよい
				if (blockmoved[i] >= blockmovecount) {
					// 周囲8マスのブロックを表すBitBoardをクリアする
					pinfo.bb[i + 10].clear();
					// 各列について計算する
					for (int x2 = 0; x2 < BOARD_WIDTH; x2++) {
						// x2 列の i 番のブロックのビットパターンと around_bb_x_table を使ってまとめてその列のすべてのブロックに対する
						// 周囲8マスのブロックのBitBoardを計算して OR をとる
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
	// turnを1増やす
	pinfo.turn++;
	return true;
}

void PlayerInfo::calchyouka() {
	// ブロック数に変更
	int onum = bb[10].pcount();


	// お邪魔ブロック数による評価（敵の数-味方の数をそのまま使う）
	hyouka.ojama = ((ojama[1] - ojama[0]) / 10) * 10 + ((ojama[1] - ojama[0]) % 10) * 0.2;
	hyouka.droppedojama = minojamanum - bb[10].pcount();
	// 致死量かどうかのチェック
	// こちらの攻撃により、この深さ以降で敵は連鎖できないものと仮定する
	// 従って、考慮に入れるのは敵のスキルによるお邪魔のみ
	// この時点で敵に致死量のお邪魔が降る場合のみ計算する
	hyouka.enedead = 0;

	if (turn <= 20 && enemayojamadropnum != 0 && enemayojamadropnum < 20) {
		hyouka.ojama += 50;
	}
	else if (onum <= 70 && enemayojamadropnum != 0 && enemayojamadropnum <= 15) {
		hyouka.ojama += enemayojamadropnum * 0.9;
	}

	// 自分が大連鎖をせず、敵が深さ１でお邪魔を降らせた場合で、
	if (getojama < 10 && enemayojamadroppeddepth > 0 && ginfo.hyoukabonusx[enemayojamadroppeddepth] == fx && ginfo.hyoukabonusr[enemayojamadroppeddepth] == fr && ginfo.hyoukabonus[enemayojamadroppeddepth] > getojama) {
//		cerr << "xxxxxx" << ginfo.hyoukabonus[1] << " " << getojama << endl;
		hyouka.ojama += ginfo.hyoukabonus[enemayojamadroppeddepth] - getojama;
	}
	//else if (getojama < 10 && enemayojamadropnum < 10 && ginfo.hyoukabonus[0] > 0 && ginfo.hyoukabonusx[enemayojamadroppeddepth] == fx && ginfo.hyoukabonusr[enemayojamadroppeddepth] == fr) {
	//	hyouka.ojama += ginfo.hyoukabonus[0] - getojama;
	//}

	if (getojama <= 15) {
		hyouka.ojama -= getojama * 0.9;
	}
	hyouka.enemaydrop = enemayojamadroppedturnnum * parameter.enemaydrophyouka + enenotojamadroppedturnnum * parameter.enenotdrophyouka;

	//if (!isenemy && skillused) {
	//	cerr << ojama[1] << "," << deadojamanum[1] << endl;
	//}
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
			hyouka.enedead = 1e10 - skillusedturn * 1e7;
		}
		// そうでない場合
		else {
			// 死亡ターンの計算
			int deadturn;
#if 0
			deadturn = (deadojamanum[1] / 10 - 1);
			// 死亡ターンまでの間に溜められるスキルで消せるブロックの数を計算。
			// スキルを稼ぐ必要があることを考慮し、1ターンあたり2個増やせるものとする// todo これ2で良い？
			int skillblock = eneskilldeleteblocknum + deadturn * 2;
			if (ojama[1] - skill_ojamatable[skillblock] >= deadojamanum[1]) {
				// なるべく早いターンで殺したいので、スキル発動ターンが速いほうを優先する
				hyouka.enedead = 1e9 - skillusedturn * 1e6;
			}
#else
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
				hyouka.enedead = 1e9 - skillusedturn * 1e6;
			}
			//if (!isenemy && skillused) {
			//	cerr << ojama[1] << "," << ojamanum << "," << skillusedturn << "," << skillblocknum << "," << deadturn << "," << skill_ojamatable[skillblocknum] << "," << deadojamanum[1] << endl;
			//	cerr << "b " << blocknum << "," << eneskillturn << "," << eneskillnotuseojamadiff << endl;
			//}
#endif
		}
	}

	// 両端の列にある5のブロックの数(かなり有効？）
//	hyouka.fiveedge = popcnt(bb[5].getx(0)) + popcnt(bb[5].getx(BOARD_WIDTH - 1));
	hyouka.fiveedge = (bb[0].pcount() - onum) * 0.01;
	// スキルによって消去可能なブロックの数
	hyouka.skilldeleteblocknum = calcskilldeleteblocknum();
	// お邪魔の数が50未満の時は、スキルは狙わない
	int mincheckonum;
	double mulskillojama;
	// ビームによる連鎖を目指している場合
	if (ginfo.skillmode) {
		mincheckonum = 0;
		mulskillojama = 1.0;
	}
	// 目指していない場合
	else {
		mincheckonum = 50;
		mulskillojama = 0.5;
	}

	if (onum >= mincheckonum) {
		// スキルを発動不可能な場合
		if (skill[0] < 80) {
			// skillojama に 消去可能なブロック数によって相手に降らせるお邪魔の数 * (スキルポイント + 1) / 81 を設定
			// 1 を足しているのはスキルポイントが0の場合でも正の評価値を与えたいため
			hyouka.skillojama = skill_ojamatable[hyouka.skilldeleteblocknum] * (skill[0] + 1.0) / 81.0 * mulskillojama;
		}
		// スキルを発動可能な場合は、スキルを発動した場合に降ってくるお邪魔の数を skillojama に設定する
		else {
			hyouka.skillojama = skill_ojamatable[hyouka.skilldeleteblocknum];
		}
	}
	else {
		hyouka.skillojama = 0;
	}
	// 敵のスキルに関する評価値
	// 敵がスキルをこのターンで使用不能な場合の敵のスキル評価
	// 使用可能な場合は ojama に組み込まれているので計算する必要はない
	// ただし、あまり評価しすぎても仕方がないので、あと3回チェインした場合に使用できるようになる
	// スキルポイントが 56 以上の場合のみ評価する
	if (!parameter.calceneskill || skill[1] < 56) {
		hyouka.eneskillojama = 0;
	}
	// スキルポイント/80 割合のさらに1/5で評価する
	// 1/2だと、この深さ落ちていないにも関わらず、この深さより前にスキルでお邪魔を落とされた場合よりも評価値が悪くなるケースがあった
	// そもそもここは計算しないほうがいいかも？
	else if (skill[1] < 80) {
		hyouka.eneskillojama = skill_ojamatable[eneskilldeleteblocknum] * skill[1] / 80.0 / 5.0;
		// スキルを使った場合の評価値と二重計算にならないように、相手に送るお邪魔の差異の分だけ引く
		hyouka.eneskillojama -= eneskillnotuseojamadiff;
		//hyouka.normalblocknum = eneskillnotuseojamadiff;
		if (hyouka.eneskillojama < 0) {
			hyouka.eneskillojama = 0;
		}
	}
	else {
		// こちらは確実に次で降ってくるのでそのままの値で計算する
		hyouka.eneskillojama = skill_ojamatable[eneskilldeleteblocknum];
		// スキルを使った場合の評価値と二重計算にならないように、相手に送るお邪魔の差異の分だけ引く
		hyouka.eneskillojama -= eneskillnotuseojamadiff;
		//hyouka.normalblocknum = eneskillnotuseojamadiff;
		//		hyouka.normalblocknum = eneskillnotuseojamadiff;
		if (hyouka.eneskillojama < 0) {
			hyouka.eneskillojama = 0;
		}
	}

	// ブロックが致死量に迫っている場合の評価値
	hyouka.limit = 0;
	// スキルが使えない場合のみ計算する
	if (skill[0] < 80) {
		for (int i = 0; i < BOARD_WIDTH; i++) {
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
				hyouka.limit += parameter.hyouka_limit_0;
			}
		}
	}
	double chainhyouka = 0;
	if (maxchain > 3) {
		if (firstenedroppedojamanum <= 0 && maxchain <= parameter.firstminchainnum) {
			chainhyouka = parameter.firstminchainminus;
		}
		else if (firstenedroppedojamanum <= 20 && maxchain <= parameter.secondminchainnum) {
			chainhyouka = parameter.secondminchainminus;
		}
	}
	double penalty = 0;
	if (fchain > maxfirstchain && enemayojamadropnum < chainpenalty) {
		penalty = enemayojamadropnum - chainpenalty;
	}
	double bonus = 0;
	if (fx == firstx && fr == firstr) {
		bonus = firstbonus;
	}


	// 敵は死亡していないのでこの評価値を 0 とする
	hyouka.enedeaddepth = 0;
	// 最終評価値の計算
	//hyouka.total = hyouka.ojama + hyouka.skillojama - hyouka.fiveedge / 10 + hyouka.limit;
	hyouka.total = hyouka.ojama + hyouka.droppedojama + hyouka.skillojama + hyouka.limit + hyouka.fiveedge + hyouka.enedead - hyouka.eneskillojama + hyouka.enemaydrop + chainhyouka + penalty + bonus;
}

// ビームサーチの場合の評価値の計算
void PlayerInfo::calcbeamhyouka() {
	PlayerInfo pinfo;
	bool changehash = true;
	double maxhyouka = -100000;
	int mchain = 0;
	int maxnormalblocknum = 0;
	int maxxr;
//	int enedeaddepth = 0;
//	hyouka.total = -10000000;
//	hyouka.enedeaddepth = 0;
	for (x = 0; x <= 8; x++) {
		for (r = 0; r < 4; r++) {
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶg
			if (!action(*this, pinfo, changehash)) {
				continue;
			}
			int cnum = pinfo.chain;
			int blnum = pinfo.bb[0].pcount() - pinfo.bb[10].pcount();
			// 新しい評価値。ターン数も考慮に入れる。ただし、最後の (hyouka.total < maxhyouka)のチェックがなければ無意味
			double h;
			if (parameter.usebeamhyoukamul) {
				h = chain_ojamatable[cnum] * 100 + blnum;// +rnd_01(mt);
				// 最初のみ上3つ分に積まないように制限する（積みすぎるとカウンターしずらくなるため）
				if (ginfo.firstbeamcheck) {
					for (int i = 0; i < BOARD_WIDTH; i++) {
						if (blocknum[i] >= GAMEOVER_Y - 3) {
							h = 0;
							break;
						}
					}
				}
			}
			else {
				// 古い評価値
				h = cnum * 100 + blnum;// +rnd_01(mt);
			}
			if (maxhyouka < h) {
				maxhyouka = h;
				mchain = cnum;
				maxnormalblocknum = blnum;
				maxxr = x * 10 + r;
			}
			//if (hyouka.total < h) {
			//	hyouka.total = h;
			//	hyouka.chain = pinfo.chain;
			//	hyouka.normalblocknum = blnum;
			//}
		}
	}
	if (this->maxchain < mchain) {
		this->maxchain = mchain;
	}

	// maxhyoukaが負の場合は死んでいる
	if (maxhyouka < 0) {
		hyouka.total = -100000;
	}
	// これまでの最高の評価値を採用する。これにより、最大連鎖のターン数を考慮に入れることができるようになる
	// なお、ここの if の条件をなくせば、各ターンごとの最高値を評価値とすることになる
	else if (!parameter.usebeamhyoukamul) {
		hyouka.total = maxhyouka;
		hyouka.chain = mchain;
//		hyouka.normalblocknum = maxnormalblocknum;
		hyouka.normalblocknum = maxxr;
	}
	else {
		hyouka.total = hyouka.total * parameter.beamhyoukamul + maxhyouka;
		hyouka.chain = mchain;
//		hyouka.normalblocknum = maxnormalblocknum;
		hyouka.normalblocknum = maxxr;


		if (hyouka.maxchain < mchain) {
			hyouka.maxchain = mchain;
		}
		if (maxhyouka == 0) {
			hyouka.total = 0;
		}
		else {
			hyouka.total = chain_ojamatable[hyouka.maxchain] * 1000 + maxhyouka;
		}
	}
}

// 探索深さ
int searchDepth;
int mysearchDepth, enesearchDepth;

// double の無限大
constexpr double INF = std::numeric_limits<double>::infinity();

// 置換表
TT_TABLE tt;

int ai2depth;

// ai。searchDepth までの深さの局面の全探索。深さ優先探索
// 各深さのデータは引数では渡さずに、大域変数の history 配列に格納する
// 味方と、敵の局面の双方で同じ関数を使う。
// 味方と敵の区別は大域変数 isenemy で区別する（引数にしたほうが良いか？）
// 味方の場合は、searchDepth で評価関数を計算し、評価値が最高のルートノードからの
// 局面を besthistory に記録する
// 敵の場合は、味方の探索深さ-1まで探索を行う。また、各ノードでこちらに影響する条件の最大値である
// 「お邪魔を降らせた数」、「相手のスキルを減らした数」を記録する（評価関数ではない）
// また、敵の場合は各ノードで、お邪魔が降る場合と降らない場合、スキルを使う場合と使わない場合に分けて
// 最大値を estatus に記録する
// estatus は、味方の探索時に、味方のお邪魔を増やしたり、スキルを減らしたりする処理で使用する
//// 引数 depth は探索する深さ
double ai(int depth, PlayerInfo *history, PlayerInfo *bhistory, PlayerInfo *bbeamhistory, HyoukaData& bhyouka, HyoukaData& bbeamhyouka, EnemyStatusData& estatusdata) {
	// この深さのノードのプレーヤーの情報への参照
	PlayerInfo& pinfoorig = history[depth];
	// pinfoorig に対してプレーヤーが行動を行った結果の局面のプレーヤーの情報への参照
	PlayerInfo& pinfo = history[depth + 1];
#ifdef USE_ZORBIST
	bool changehash = depth != searchDepth - 1;
#else
	static constexpr bool changehash = false;
#endif

	aicount++;

	// お邪魔落下処理(isenemyの中では、これまでの行動でお邪魔が降ったかどうかの情報を使うので、それが終わってからこの処理を行う必要がある）
	if (depth == 0 || depth != ai2depth) {
		pinfoorig.ojamadrop(depth, changehash);
	}

	// リーフノードまたは、最終ターンを過ぎた場合の処理
	if (depth == searchDepth || pinfoorig.turn == MAX_TURN) {
		// 味方の場合は評価値を計算し、これまでの評価値の最大値を超えた場合は、besthistory にこれまでの行動を記録する
		pinfoorig.calchyouka();
		if (parameter.ai2) {
			return pinfoorig.hyouka.total;
		}
		HyoukaData& hyouka = pinfoorig.hyouka;
		if (hyouka.total > bhyouka.total) {
			//cerr << "best " << bhyouka.total << endl;
			bhyouka = hyouka;
			memcpy(bhistory, history, sizeof(PlayerInfo) * (searchDepth + 1));
		}
		//if (ginfo.beamflag && history[0].x == ginfo.beamx && history[0].r == ginfo.beamr && hyouka.total > bestbeamhyouka.total) {
		if (history[0].x == ginfo.beamx && history[0].r == ginfo.beamr && hyouka.total > bbeamhyouka.total) {
			bbeamhyouka = hyouka;
			if (parameter.debugmsg) {
				memcpy(bbeamhistory, history, sizeof(PlayerInfo) * (searchDepth + 1));
			}
		}

		// 終了する
		return 0;
	}

	// 置換表を使って同じ局面が出てきた場合は終了する
	TT_DATA *tdata = nullptr;
	Key key;
	if (parameter.usett && depth != 0) {
		key = pinfoorig.calc_hash();
		bool found;
		tdata = tt.findcache(key, found);
		if (found) {
			return tdata->get();
		}
	}

	// この深さでの行動を表す変数への参照（x:落下位置（-1の場合はスキルを使う),r:回転数）
	int& x = pinfoorig.x;
	int& r = pinfoorig.r;
	double maxhyouka = -INF;
	// 可能な行動をすべて試す。ただし、x == -1 は スキル使用を表す
	for (x = -1; x <= 8; x++) {
		if (depth == 0 && parameter.usethread) {
			x = pinfoorig.px;
		}
		for (r = 0; r < 4; r++) {
			if (depth == 0) {
				pinfoorig.fx = x;
				pinfoorig.fr = r;
			}
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ

			if (!action(pinfoorig, pinfo, changehash)) {
				//				if (depth == 0 && parameter.usethread) {
				//					break;
				//				}
				continue;
			}
			//if (depth == 0 && x == 0) {
			//	cerr << depth << ",," << x << "," << r << endl;
			//	pinfo.dump_field();
			//}
			if (depth == 0) {
				pinfo.fchain = pinfo.chain;
			}

			// チェイン数を格納した変数への参照
			uint8_t& chain = pinfo.chain;

			// 累積お邪魔を降らした数を、チェインの数によって加算する
			pinfo.getojama += chain_ojamatable[chain];
			// 相手のお邪魔の数を増やす
			pinfo.ojama[1] += chain_ojamatable[chain];

			// チェインがあった場合(3チェイン以上あった場合の相手のスキルを減らす処理は、相手のスキル増加の後に行う必要がある）
			if (chain > 0) {
				// 自分のスキルを 8 増やす
				pinfo.skill[0] += 8;
				// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
				if (pinfo.skill[0] > 100) {
					pinfo.skill[0] = 100;
				}
			}

			// 味方の思考の場合、敵のこの深さでの行動の結果を反映させる。
			// この時点では、まだこのターンの開始時の敵のスキルポイントの情報を使用したいので、
			// 敵のスキルポイントの増減を行う前にこの処理を行う必要がある
			// 敵がスキルの使用状況にる estatus のインデックス番号を表す変数。初期値としてスキルを全く使用しないことを表す 0 を設定しておく
			int skillindex = 0;
			bool checkanotherpinfo = false;
			PlayerInfo anotherpinfo;
			if (depth < enesearchDepth) {
				// この深さで敵がスキルを使えるかどうかのチェック
				if (pinfo.skill[1] >= 80 && pinfo.eneskilldeleteblocknum > 0) {
					// 過去に敵がスキルを使用可能だったとしても、敵がスキルを使用するかどうかが不明なので、スキル使用によるスキルポイントの減少の処理は行わない
					// 従って、スキルが使える場合は、estatus のすべての可能性の中のいいとこどりをしたインデックス(4)を使用する
					skillindex = 4;
					// 使用した可能性があることを表すフラグを立てる
					pinfo.mayeneuseskill = true;
				}
				// 使えない場合は、過去に使った可能性があるかどうかで分ける
				// そうでなければ過去に使用した可能性がある場合のインデックス番号1を採用する
				// なお、pinfo.mayeneuseskill は実際にはスキルを使用不能な場合も含まれる可能性がある
				// (敵のスキルポイントは estatus の chain が 1 以上の時に換算するが、実際にはスキルが増えないケースがあるため）
				// estatusdata.data[1][pinfo.eneojamadroppeddepthbit][depth + 1].isdead が true の場合は過去に使った可能性はないので、それをチェックする
				else if (pinfo.mayeneuseskill && !estatusdata.data[1][pinfo.eneojamadroppeddepthbit][depth + 1].isdead) {
					skillindex = 1;
				}

				// 敵が死亡している場合はこれ以上探索を続ける必要はない
				if (estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].isdead) {
					// 敵が死亡した場合の評価を計算（引数には1以上を入れる必要があるので1を足す）
					pinfoorig.calcenedeadhyouka(depth + 1);
					HyoukaData& hyouka = pinfoorig.hyouka;
					// 最高値だった場合は更新して besthistory に記録
					if (!parameter.ai2) {
						if (hyouka.total > bhyouka.total) {
							bhyouka = hyouka;
							memcpy(bhistory, history, sizeof(PlayerInfo) * (depth + 1));
							//cerr << "best dead " << bhyouka.total << "," << bhyouka.enedeaddepth << "," << depth << "," << skillindex << ","
							//	<< (int)pinfo.eneojamadroppeddepthbit << "," << pinfoorig.mayeneuseskill << endl;
						}
						if (history[0].x == ginfo.beamx && history[0].r == ginfo.beamr && hyouka.total > bbeamhyouka.total) {
							bbeamhyouka = hyouka;
							if (parameter.debugmsg) {
								memcpy(bbeamhistory, history, sizeof(PlayerInfo) * (searchDepth + 1));
							}
						}
					}
					else {
						if (depth == 0) {
							bhyouka.total = hyouka.total;
							bhistory[0] = history[0];
							if (parameter.debugmsg) {
								bhistory[1] = pinfo;
							}
						}
					}
					// それ以上探索する必要はないので終了
					if (parameter.usett && depth != 0) {
						tdata->set(key, pinfoorig.hyouka.total);
					}
					if (parameter.debugmsg && parameter.ai2 && depth == ai2depth) {
						bhistory[depth + 1] = pinfo;
					}

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


				// 敵がここで落とす可能性のあるお邪魔の最大数
				int enemaydropnum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama - pinfo.enemayojamadropnum;
				int enemayskillminus = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxskillminus - pinfo.enemayskillminusnum;
				// 敵が enemaydropnum のお邪魔を落とす前の相対お邪魔数
				int prevojamanum = pinfo.ojama[0] - pinfo.ojama[1];
				// 敵が enemaydropnum のお邪魔を落とした後の相対お邪魔数
				int nextojamanum = prevojamanum + enemaydropnum;
				// enemaydropnum のお邪魔が落ちることによって、次のターンの状況が変化するかどうか
				// 自分にお邪魔が落ちてくるようになる場合 または、 敵にお邪魔が落ちなくなる場合を調べる
				// 敵がこれまで落とす可能性のあるお邪魔を 6 個以上降らせた場合、そのあとにお邪魔を大量に降らすことはほぼ不可能なので、それも考慮する
				// もし、状況が変化する場合は、最大限お邪魔を落とした場合と、そうでない場合の両方を計算し、評価値の低いほうを採用する
				if (parameter.ai3) {
					if (pinfo.enemayojamadropnum >= 6) {
						enemaydropnum = 0;
//						enemayskillminus = 0;
					}
					else if ((prevojamanum < 10 && nextojamanum >= 10) || (prevojamanum <= -10 && nextojamanum > -10)) {
						checkanotherpinfo = true;
						// お邪魔を落とせるか落とせないかを選択できたターン数
						pinfo.enemayojamadroppedturnnum++;
						anotherpinfo = pinfo;
						anotherpinfo.enenotojamadroppedturnnum++;
					}
					else {
						enemaydropnum = 0;
//						enemayskillminus = 0;
					}
				}
				else if ((prevojamanum < 10 && nextojamanum >= 10) || (prevojamanum <= -10 && nextojamanum > -10)) {
					pinfo.enemayojamadroppedturnnum++;
				}
				if (enemaydropnum >= 6) {
					pinfo.enemayojamadroppeddepth = depth + 1;
				}
				pinfo.ojama[0] += enemaydropnum;
				pinfo.enemayojamadropnum += enemaydropnum;
				// 場合によってはマイナスになる場合もある。その場合は0に補正する（これでOK?相手のお邪魔増やさなくても良いか？）
				if (pinfo.ojama[0] < 0) {
					pinfo.ojama[0] = 0;
				}
				// 味方のスキルを減らす。スキルの範囲チェックはこの後でまとめて行う
				pinfo.skill[0] -= enemayskillminus;
				pinfo.enemayskillminusnum += enemayskillminus;
//				estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].skillminus;
				// 相手がチェインしていた場合は相手のスキルを増やす
				if (estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].ischain) {
					pinfo.skill[1] += 8;
					// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
					if (pinfo.skill[1] > 100) {
						pinfo.skill[1] = 100;
					}
				}
				pinfo.eneskilldeleteblocknum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].skilldeleteblocknum;
				// 敵がスキルを使わなかった場合の最小ブロック数
				pinfo.eneskillnotuseminblocknum = estatusdata.data[0][pinfo.eneojamadroppeddepthbit][depth + 1].minblocknum;
				// 敵がスキルを使わなかった場合の、maxojama の差異
				pinfo.eneskillnotuseojamadiff = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama - estatusdata.data[0][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama;
				pinfo.minojamanum = estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].minojamanum;
			}
			// 敵の行動を計算していない場合
			else {
				// 2を足しておく todo: これ2で良い？
				pinfo.eneskilldeleteblocknum += 2;
				// 消えるか増えるか不明なので、相手にお邪魔が降る場合を除き、eneskillnotuseminblocknumは変化させない
				//if (pinfo.ojama[1] - pinfo.ojama[0] >= 10) {
				//	pinfo.eneskillnotuseminblocknum += 10;
				//	pinfo.minojamanum += 10;
				//}
				// チェインしたことにしておく
				pinfo.skill[1] += 8;
				if (pinfo.skill[1] > 100) {
					pinfo.skill[1] = 100;
				}
			}

			pinfo.calcskillandojama();
			//if (depth == 1 &&
			//	history[0].x == 7 && history[0].r == 2 &&
			//	history[1].x == 8 && history[1].r == 2) {
			//	pinfo.dump_field();
			//	cerr << "y " <<x << "," << r << " " << ai2depth  << endl;
			//}
			// 次の深さで探索を行う
			if (parameter.ai2) {
				double hyouka = ai(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
				double anotherhyouka = 0;
				// 敵がお邪魔を降らさない場合の評価値を計算する
				if (parameter.ai3) {
					PlayerInfo pinfobak = pinfo;
					if (checkanotherpinfo) {
						pinfo = anotherpinfo;
						pinfo.calcskillandojama();
						anotherhyouka = ai(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
						if (hyouka > anotherhyouka) {
							hyouka = anotherhyouka;
//							pinfo = anotherpinfo;
						}
						else {
							pinfo = pinfobak;
						}
					}
					else {
						pinfo = pinfobak;
					}
				}
//				if (depth == ai2depth
////					&& history[0].x == 4 && history[0].r == 2
////					&& history[1].x == 4 && history[1].r == 2
////					&& history[2].x == 4 && history[2].r == 0
////					&& history[3].x == 4 && history[3].r == 0
//					) {
//					cerr << ai2depth << " x " << x << "," << r << " " << ai2depth << " ," << hyouka << "," << maxhyouka << "," << skillindex << "," << (int)pinfo.eneojamadroppeddepthbit << "," << estatusdata.data[skillindex][pinfo.eneojamadroppeddepthbit][depth + 1].maxojama << endl;
//					//pinfo.dump_field();
//				}
				//if (depth == 1 &&
				//	history[0].x == 7 && history[0].r == 2  &&
				//	history[1].x == 8 && history[1].r == 2) {
				//	pinfo.dump_field();
				//	cerr << x << "," << r << " " << ai2depth << " ," << hyouka << "," << maxhyouka << endl;
				//}
				//if (depth == 0 && x == 0) {
				//	cerr << depth << "," << x << "," << r << " " << maxhyouka << " " << hyouka << " " << bhyouka.total << endl;
				//}
				if (maxhyouka < hyouka) {
					maxhyouka = hyouka;
					//if (parameter.debugmsg) {
					//	memcpy(bhistory + depth, history + depth, sizeof(PlayerInfo) * (searchDepth + 1 - depth));
					//}
					if (depth == 0) {
						bhyouka.total = hyouka;
						bhistory[0] = history[0];
						if (parameter.debugmsg) {
							bhistory[1] = pinfo;
						}
					}
					if (parameter.debugmsg && depth > 0 && depth == ai2depth) {
						bhistory[depth] = pinfoorig;
						bhistory[depth + 1] = pinfo;
					}
				}
				if (depth == 0) {
					if (x == ginfo.beamx && r == ginfo.beamr) {
						bbeamhyouka.total = hyouka;
						bbeamhistory[0] = history[0];
						//cerr << "xxxxxxxxxx" << endl;
						//pinfo.dump_field();
						if (parameter.debugmsg) {
							bbeamhistory[1] = pinfo;
						}
						//if (parameter.debugmsg) {
						//	memcpy(bbeamhistory, history, sizeof(PlayerInfo) * (searchDepth + 1));
						//}
					}
				}
			}
			else {
				ai(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
			}
		}
		if (depth == 0 && parameter.usethread) {
			break;
		}
	}
	if (parameter.usett && depth != 0) {
		tdata->set(key, maxhyouka);
	}
	return maxhyouka;
}

void ai_enemy(int depth, PlayerInfo *history, PlayerInfo *bhistory, PlayerInfo *bbeamhistory, HyoukaData& bhyouka, HyoukaData& bbeamhyouka, EnemyStatusData& estatusdata) {
	// この深さのノードのプレーヤーの情報への参照
	PlayerInfo& pinfoorig = history[depth];
	// pinfoorig に対してプレーヤーが行動を行った結果の局面のプレーヤーの情報への参照
	PlayerInfo& pinfo = history[depth + 1];
#ifdef USE_ZORBIST
	bool changehash = depth != searchDepth - 1;
#else
	static constexpr bool changehash = false;
#endif

	aicount++;

	

	// 敵の行動の状態を記録する変数への参照
	// 「前の行動でスキルを使ったかどうか」、「これまでの行動でお邪魔が降ったかどうか」、「現在の深さ」によって記録する場所を変える
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
	if (pinfoorig.skillusedinthisdepth && pinfoorig.getskillojama > estat.maxskillojama) {
		estat.maxskillojama = pinfoorig.getskillojama;
	}
//	if (pinfoorig.skillused) {
//		estat.maxskillojama = 20;// pinfoorig.getskillojama;
//	}
	//if (pinfoorig.skill[0] > estat.maxskill) {
	//	estat.maxskill = pinfoorig.skill[0];
	//}
	// これまで減らした相手のスキルポイントの最大値の記録
	if (pinfoorig.getskillminus > estat.maxskillminus) {
		estat.maxskillminus = pinfoorig.getskillminus;
	}
	// スキルで消せるブロック数の最大値
	int skilldeleteblocknum = pinfoorig.calcskilldeleteblocknum();
	if (skilldeleteblocknum > estat.skilldeleteblocknum) {
		estat.skilldeleteblocknum = skilldeleteblocknum;
	}
	// ブロックの最小数
	int minblocknum = pinfoorig.bb[0].pcount();
	if (minblocknum < estat.minblocknum) {
		estat.minblocknum = minblocknum;
	}
	// お邪魔ブロックの最小値
	int minojamanum = pinfoorig.bb[10].pcount();
	if (minojamanum < estat.minojamanum) {
		estat.minojamanum = minojamanum;
	}

	// お邪魔落下処理(isenemyの中では、これまでの行動でお邪魔が降ったかどうかの情報を使うので、それが終わってからこの処理を行う必要がある）
	pinfoorig.ojamadrop(depth, changehash);

	// リーフノードまたは、最終ターンを過ぎた場合の処理
	if (depth == searchDepth || pinfoorig.turn == MAX_TURN) {

		// 終了する
		return;
	}

	// 置換表を使って同じ局面が出てきた場合は終了する
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
	int& x = pinfoorig.x;
	int& r = pinfoorig.r;
	double maxhyouka = -INF;
	// 可能な行動をすべて試す。ただし、x == -1 は スキル使用を表す
	for (x = -1; x <= 8; x++) {
		if (depth == 0 && parameter.usethread) {
			x = pinfoorig.px;
		}
		for (r = 0; r < 4; r++) {
			// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
			if (!action(pinfoorig, pinfo, changehash)) {
				continue;
			}

			// チェイン数を格納した変数への参照
			uint8_t& chain = pinfo.chain;
			// 累積お邪魔を降らした数を、チェインの数によって加算する
			pinfo.getojama += chain_ojamatable[chain];
			// 相手のお邪魔の数を増やす
			pinfo.ojama[1] += chain_ojamatable[chain];

			// チェインがあった場合(3チェイン以上あった場合の相手のスキルを減らす処理は、相手のスキル増加の後に行う必要がある）
			if (chain > 0) {
				// 自分のスキルを 8 増やす
				pinfo.skill[0] += 8;
				// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
				if (pinfo.skill[0] > 100) {
					pinfo.skill[0] = 100;
				}
			}

			// 味方の思考の場合、敵のこの深さでの行動の結果を反映させる。
			// この時点では、まだこのターンの開始時の敵のスキルポイントの情報を使用したいので、
			// 敵のスキルポイントの増減を行う前にこの処理を行う必要がある
			// 敵がスキルの使用状況にる estatus のインデックス番号を表す変数。初期値としてスキルを全く使用しないことを表す 0 を設定しておく
			int skillindex = 0;

			// estatus のスキルの使用に関するインデックス番号の計算
			pinfo.skillindex = 0;
			if (pinfo.skillusedinthisdepth) {
				pinfo.skillindex = 2;
			}
			else if (pinfo.skillused) {
				pinfo.skillindex = 1;
			}
			// ここまで来たという事はこの深さで生存する行動があるということなので、生存フラグを立てる
			estatusdata.data[pinfo.skillindex][pinfo.ojamadroppeddepthbit][depth + 1].isdead = false;
			// 3チェイン以上の場合
			if (chain >= 3) {
				// 相手のスキル減少値の計算（相手のスキル増加、増加あふれの処理の後に行う必要あり）
				int skillminus = 12 + chain * 2;
				// 相手のスキルを減らす
				pinfo.skill[1] -= skillminus;
				// 累積相手のスキル減少値を増やす
				pinfo.getskillminus += skillminus;
			}

			// スキルの範囲チェック
			for (int i = 0; i < 2; i++) {
				if (pinfo.skill[i] < 0) {
					pinfo.skill[i] = 0;
				}
				else if (pinfo.skill[i] > 100) {
					pinfo.skill[i] = 100;
				}
			}
			// お邪魔のチェック
			// 両方とも正の場合
			if (pinfo.ojama[0] > 0 && pinfo.ojama[1] > 0) {
				// 大きいほうから小さいほうの値を引き、小さいほうを0とする
				if (pinfo.ojama[0] > pinfo.ojama[1]) {
					pinfo.ojama[0] -= pinfo.ojama[1];
					pinfo.ojama[1] = 0;
				}
				else {
					pinfo.ojama[1] -= pinfo.ojama[0];
					pinfo.ojama[0] = 0;
				}
			}

			// 次に降ってくるお邪魔の数を記録しておく
			int ojamanum = pinfo.ojama[0];
			// 次の深さで探索を行う
			ai_enemy(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
			// 敵の思考で、邪魔が降ってなかった場合は、降らす場合も計算する
			if (ojamanum < 10) {
				pinfo.ojama[0] = 10;
				ai_enemy(depth + 1, history, bhistory, bbeamhistory, bhyouka, bbeamhyouka, estatusdata);
			}
		}
		if (depth == 0 && parameter.usethread) {
			break;
		}
	}
	return;
}

void ai_thread() {
	EnemyStatusData enemystatusdata_thread[11];
	PlayerInfo thread_history[11][MAX_DEPTH + 1];
	PlayerInfo thread_besthistory[11][MAX_DEPTH + 1];
	PlayerInfo thread_bestbeamhistory[11][MAX_DEPTH + 1];
	HyoukaData besthyouka_thread[11];
	HyoukaData bestbeamhyouka_thread[11];
	std::vector<std::thread> threads;
	// todo!!! 1手目の行動後の結果が同じものが存在した場合、beamx, beamr の評価チェックが
	// 行われず、-inf になる可能性がある。あらかじめ、1手目の beamx, beamr と同じものを計算しておく必要あり？ 
	//// 1手目の重複チェック用
	//PlayerInfo& pinfoorigcheck = history[0];
	//// おじゃまを降らしておく
	//pinfoorigcheck.ojamadrop(0, true);
	// 初手のみ、結果が同じ場合は
	ai2depth = 0;
	for (int x = -1; x <= 8; x++) {
		thread_history[x + 1][0] = history[0];
		PlayerInfo& pinfoorig = thread_history[x + 1][0];
		besthyouka_thread[x + 1].total = -INF;
		bestbeamhyouka_thread[x + 1].total = -INF;
		pinfoorig.px = x;
		if (!isenemy) {
			//// 置換表を使う場合、同じ局面が出てきた場合は終了する
			//if (parameter.usett) {
			//	PlayerInfo pinfo;
			//	// 行動させる
			//	if (!action(pinfoorigcheck, pinfo, true)) {
			//		continue;
			//	}
			//	// 他と重複しないようにターンは1000とする
			//	pinfo.turn = 1000;
			//	TT_DATA *tdata;
			//	Key key = pinfoorig.calc_hash();
			//	bool found;
			//	tdata = tt.findcache(key, found);
			//	if (found) {
			//		continue;
			//	}
			//	tdata->set(key, 1);
			//}
			threads.emplace_back(ai, 0, thread_history[x + 1], thread_besthistory[x + 1], thread_bestbeamhistory[x + 1], std::ref(besthyouka_thread[x + 1]), std::ref(bestbeamhyouka_thread[x + 1]), std::ref(enemystatusdata));
		}
		else {
			enemystatusdata_thread[x + 1].clear(searchDepth);
			threads.emplace_back(ai_enemy, 0, thread_history[x + 1], thread_besthistory[x + 1], thread_bestbeamhistory[x + 1], std::ref(besthyouka_thread[x + 1]), std::ref(bestbeamhyouka_thread[x + 1]), std::ref(enemystatusdata_thread[x + 1]));
		}
//		threads.emplace_back(ai, 0, thread_history[x + 1], thread_besthistory[x + 1], thread_bestbeamhistory[x + 1], std::ref(besthyouka_thread[x + 1]), std::ref(bestbeamhyouka_thread[x + 1]), std::ref(enemystatusdata));
	}
	for (auto& t : threads) {
		t.join();
	}
	double maxbesthyouka = -INF;
	double maxbestbeamhyouka = -INF;

	for (int x = -1; x <= 8; x++) {
		if (!isenemy) {
			//cerr << "aa " << x << " " << besthyouka_thread[x + 1].total << endl;
			if (!parameter.ai2) {
				if (maxbesthyouka < besthyouka_thread[x + 1].total) {
					besthyouka = besthyouka_thread[x + 1];
					maxbesthyouka = besthyouka.total;
					memcpy(besthistory, thread_besthistory[x + 1], sizeof(PlayerInfo) * (searchDepth + 1));
				}
				if (maxbestbeamhyouka < bestbeamhyouka_thread[x + 1].total) {
					bestbeamhyouka = bestbeamhyouka_thread[x + 1];
					maxbestbeamhyouka = bestbeamhyouka.total;
					memcpy(bestbeamhistory_log, thread_bestbeamhistory[x + 1], sizeof(PlayerInfo) * (searchDepth + 1));
				}
			}
			else {
				if (maxbesthyouka < besthyouka_thread[x + 1].total) {
					besthyouka = besthyouka_thread[x + 1];
					maxbesthyouka = besthyouka.total;
//					besthistory[0] = thread_besthistory[x + 1][0];
					memcpy(besthistory, thread_besthistory[x + 1], sizeof(PlayerInfo) * (searchDepth + 1));
//					besthistory[0].dump_field();
				}
				if (maxbestbeamhyouka < bestbeamhyouka_thread[x + 1].total) {
					bestbeamhyouka = bestbeamhyouka_thread[x + 1];
					maxbestbeamhyouka = bestbeamhyouka.total;
					memcpy(bestbeamhistory_log, thread_bestbeamhistory[x + 1], sizeof(PlayerInfo) * (searchDepth + 1));
				}
			}
		}
		else {
			if (x == -1) {
				enemystatusdata = enemystatusdata_thread[0];
			}
			else {
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < (1 << searchDepth); j++) {
						for (int k = 1; k <= searchDepth; k++) {
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
	if (!isenemy && parameter.debugmsg && parameter.ai2) {
		bool usettbak = parameter.usett;
		PlayerInfo besthistorybak[MAX_DEPTH + 1];
		
		parameter.usett = false;
//		tt.clear();
		for (ai2depth = 1; ai2depth < searchDepth; ai2depth++) {
			history[ai2depth] = besthistory[ai2depth];
			ai(ai2depth, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		memcpy(besthistorybak, besthistory, sizeof(PlayerInfo) * (MAX_DEPTH + 1));

		history[0] = bestbeamhistory_log[0];
		besthistory[1] = bestbeamhistory_log[1];
		for (ai2depth = 1; ai2depth < searchDepth; ai2depth++) {
			history[ai2depth] = besthistory[ai2depth];
			ai(ai2depth, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		memcpy(bestbeamhistory_log + 1, besthistory + 1, sizeof(PlayerInfo) * (MAX_DEPTH + 1 - 2));

		parameter.usett = usettbak;
		memcpy(besthistory, besthistorybak, sizeof(PlayerInfo) * (MAX_DEPTH + 1));

	}
}

vector <PlayerInfo, MyAllocator<PlayerInfo>> beam_pdata[MAX_BEAM_DEPTH][MAX_CHAIN + 1];
PlayerInfo firstbestbeamhistory[MAX_BEAM_DEPTH];
PlayerInfo bestbeamhistory[MAX_BEAM_DEPTH];
int beamDepth;
int beamWidth;
int beamChainWidth;

int beamsearch(bool returndepth = false, int beamdropdepth = 0, int beamfirstdropnum = 0, bool isskill = false) {
	PlayerInfo pinfo;
	ginfo.istimeout = false;
	ginfo.firstmaxchaindepth = -1;
	// 置換表の初期化
	if (parameter.usett) {
		tt.clear();
	}
	int maxchain = 0;
	int maxchaindepth = 0;
	for (int i = 0; i < MAX_CHAIN; i++) {
		beam_pdata[0][i].clear();
	}
	history[0].ojamadrop(0, true);
	beam_pdata[0][0].push_back(history[0]);
	bool changehash = true;
	beamDepth = parameter.beamsearchDepth;
	beamWidth = parameter.beamsearchWidth;
	beamChainWidth = parameter.beamsearchChainWidth;
	int depth;
	bool foundbeamchainmax = false;
	if (parameter.debugmsg) {
		cerr << endl;
	}
	for (depth = 0; depth < beamDepth; depth++) {
		if (ginfo.t.time() >= parameter.firsttimelimit) {
			ginfo.istimeout = true;
			break;
		}
		int count = 0;
		for (int i = 0; i < MAX_CHAIN; i++) {
			beam_pdata[depth + 1][i].clear();
		}
		bool ismaxchain = true;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			size_t size = beam_pdata[depth][chain].size();
			if (size == 0) {
				continue;
			}
			if (ginfo.t.time() >= parameter.firsttimelimit) {
				ginfo.istimeout = true;
				break;
			}
			int chain_count = 0;
			// 評価値の順にソートする
			// partial_sort は速くなかったので使わない
			//if (size > beamChainWidth) {
			//	std::partial_sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].begin() + beamChainWidth, beam_pdata[depth][chain].end());
			//}
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());

			if (size >= beamChainWidth * 1.2) {
				beam_pdata[depth][chain].resize(static_cast<int>(beamChainWidth * 1.2));
				beam_pdata[depth][chain].shrink_to_fit();
			}

			int pinfoorigcount = 0;
			for (auto& pinfoorig : beam_pdata[depth][chain]) {
				if (ginfo.t.time() >= parameter.firsttimelimit) {
					ginfo.istimeout = true;
					break;
				}
				pinfoorigcount++;
				if (chain_count == 0) {
					if ((ismaxchain && parameter.turndebugmsg) && !parameter.checkbeamchain && ginfo.turn == 0) {
							cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " cc " << setw(3) << chain_count << "/" << setw(5) << size << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig.hyouka.total << " " << defaultfloat << setw(4) << pinfoorig.hyouka.chain << " " << setw(4) << pinfoorig.hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms " << endl;
					}
				}
//				pinfoorig.ojamadrop(depth, changehash);
				int &x = pinfoorig.x;
				int &r = pinfoorig.r;
				// スキルは使わないので x は 0 から始める
				for (x = parameter.beamminx; x <= parameter.beammaxx; x++) {
					for (r = 0; r < 4; r++) {
						if (depth == 0 && ginfo.checkonlybeamxr && (x != ginfo.beamx || r != ginfo.beamr)) {
							continue;
						}
						// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ
						if (!action(pinfoorig, pinfo, changehash)) {
							continue;
						}
						// スキルポイントや相手に降らすお邪魔は連鎖の構築には関係ないので計算しない
						//if (pinfo.chain > 0) {
						//	pinfo.skill[0] += 8;
						//	if (pinfo.skill[0] > 100) {
						//		pinfo.skill[0] = 100;
						//	}
						//}
						if (depth + 1 == beamdropdepth && beamfirstdropnum > 0) {
							pinfo.ojama[0] += beamfirstdropnum;
							pinfo.checkojama();
						}

						pinfo.ojamadrop(depth + 1, true);
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
						if (!isskill) {
							pinfo.calcbeamhyouka();
						}
						else {
							if (chain > 0) {
								// 自分のスキルを 8 増やす
								pinfo.skill[0] += 8;
								// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
								if (pinfo.skill[0] > 100) {
									pinfo.skill[0] = 100;
								}
							}
							pinfo.calcbeamhyouka_skill();
						}
						aicount += 36;
						pinfo.px = x;
						pinfo.pr = r;
						beam_pdata[depth + 1][pinfo.hyouka.chain].push_back(pinfo);
						if (pinfo.hyouka.chain > maxchain) {
							maxchain = pinfo.hyouka.chain;
							maxchaindepth = depth + 1;
						}
					}
				}
				chain_count++;
				count++;
				if (chain_count >= beamChainWidth || count >= beamWidth) {
					break;
				}
			}
			//beam_pdata[depth][chain].resize(pinfoorigcount);
			//beam_pdata[depth][chain].shrink_to_fit();
			ismaxchain = false;
			if (count >= beamWidth) {
				break;
			}
		}

		if (parameter.beamChainmax2 > 0 && maxchain >= parameter.beamChainmax2) {
			depth++;
			break;
		}

		if (!foundbeamchainmax && parameter.beamChainmax > 0 && maxchain >= parameter.beamChainmax) {
			ginfo.firstmaxchaindepth = depth + 1;
			if (parameter.stopbeamChainmax) {
				depth++;
				break;
			}
			foundbeamchainmax = true;
			// parameter.beamChainmax 未満の次のbeamのデータを削除する
			for (int i = 0; i < maxchain - parameter.beamChainmaxminus ; i++) {
				beam_pdata[depth + 1][i].clear();
			}
			parameter.beamhyoukamul = parameter.nextbeamhyoukamul;
		}

		if (count == 0) {
			depth--;
			break;
		}
	}
	if (depth < 0) {
		depth = 0;
	}

	beamDepth = depth;
	ginfo.maxchaindepth = maxchaindepth;
	double maxhyouka = -INF;
	for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
		if (beam_pdata[depth][chain].size() > 0) {
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());
			if (beam_pdata[depth][chain].begin()->hyouka.total > maxhyouka) {
				bestbeamhistory[depth] = *beam_pdata[depth][chain].begin();
				maxhyouka = bestbeamhistory[depth].hyouka.total;
			}
			if (chain > maxchain) {
				maxchain = chain;
				maxchaindepth = depth;
			}
			// 
			if (ginfo.t.time() >= parameter.beamtimelimit) {
				ginfo.istimeout = true;
				break;
			}
			if (parameter.beamChainmax2 > 0) {
				break;
			}
			break;
		}
	}

	if (!parameter.checkbeamchain) {
		if (parameter.turndebugmsg) {
			if (ginfo.turn == 0) {
				cerr << "depth " << setw(2) << beamDepth << " c " << setw(3) << maxchain << " hyouka " << fixed << setprecision(2) << setw(7) << bestbeamhistory[depth].hyouka.total << " " << defaultfloat << setw(4) << bestbeamhistory[depth].hyouka.chain << " " << setw(4) << bestbeamhistory[depth].hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms" << endl;
			}
			else {
				cerr << "bd " << beamDepth << " mch " << maxchain << "/" << maxchaindepth << " t " << setw(5) << ginfo.t.time() << " ";
			}
		}
	}
	else {
		// checkbeamchain の場合はdepthを返す（todo:beamDepth に depth を代入しているのだから、これはいらない？）
		cout << bestbeamhistory[depth].hyouka.total << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << bestbeamhistory[depth].hyouka.normalblocknum << "\t" << maxchain << "\t" << maxchaindepth << "\t";
		cerr << bestbeamhistory[depth].hyouka.total << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << bestbeamhistory[depth].hyouka.normalblocknum << "\t" << maxchain << "\t" << maxchaindepth << "\t";
		return depth;
	}

	PlayerInfo *ptr = bestbeamhistory[depth].prev;
	while (ptr) {
		depth--;
		bestbeamhistory[depth] = *ptr;
		ptr = ptr->prev;
	}
	if (parameter.debugmsg) {
		dump_fields(bestbeamhistory, beamDepth, true);
	}
	//for (int i = 0; i < MAX_BEAM_DEPTH; i++) {
	//	for (int j = 0; j <= MAX_CHAIN; j++) {
	//		beam_pdata[i][j].clear();
	//		beam_pdata[i][j].shrink_to_fit();
	//	}
	//}
	return maxchain;
}

void beamsearch_pdata(vector<PlayerInfo, MyAllocator<PlayerInfo>>& bpdata, int depth, size_t size, size_t maxnum, int beamdropdepth, int beamfirstdropnum, bool isskill, bool checkignore) {
	PlayerInfo pinfo;
	//std::sort(bpdata.begin(), bpdata.end());

	//if (size >= static_cast<int>(beamChainWidth * 1.2)) {
	//	bpdata.resize(static_cast<int>(beamChainWidth * 1.2));
	//	bpdata.shrink_to_fit();
	//}
	int totalcount = 0;
	int count = 0;
	for (auto& pinfoorig : bpdata) {
		if (ginfo.t.time() >= parameter.firsttimelimit || count > maxnum) {
			if (count <= maxnum) {
				ginfo.istimeout = true;
			}
			break;
		}
		if (checkignore && pinfoorig.ignore) {
			continue;
		}
		totalcount++;
		//		if (chain_count == 0) {
		//			if ((ismaxchain && parameter.turndebugmsg) && !parameter.checkbeamchain && ginfo.beamflag) {
		//				cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " cc " << setw(3) << chain_count << "/" << setw(5) << size << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig.hyouka.total << " " << defaultfloat << setw(4) << pinfoorig.hyouka.chain << " " << setw(4) << pinfoorig.hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms " << endl;
		//			}
		//		}
//		pinfoorig.ojamadrop(depth, true);
		int &x = pinfoorig.x;
		int &r = pinfoorig.r;
		// スキルは使わないので x は 0 から始める
		for (x = parameter.beamminx; x <= parameter.beammaxx; x++) {
			for (r = 0; r < 4; r++) {
				if (depth == 0 && ginfo.checkonlybeamxr && (x != ginfo.beamx || r != ginfo.beamr)) {
					continue;
				}
				//if (depth == 0) {
				//	pinfoorig.fx = x;
				//	pinfoorig.fr = r;
				//}
				// pinfoorig の行動を行い、行動後の状態を pinfo に記録する関数を呼ぶ

				if (!action(pinfoorig, pinfo, true)) {
					continue;
				}
				uint8_t& chain = pinfo.chain;
				if (chain > 1) {
					pinfo.ojama[1] += chain_ojamatable[chain];
					pinfo.checkojama();
				}
				//if (depth >= 0 && depth < enesearchDepth && pinfoorig.beamdropdata.eneojamadropnum[depth + 1] > 0) {
				//	pinfo.ojama[0] += pinfo.beamdropdata.eneojamadropnum[depth + 1];
				//	if (pinfo.ojama[0] > 0 && pinfo.ojama[1] > 0) {
				//		if (pinfo.ojama[0] > pinfo.ojama[1]) {
				//			pinfo.ojama[0] -= pinfo.ojama[1];
				//			pinfo.ojama[1] = 0;
				//		}
				//		else {
				//			pinfo.ojama[1] -= pinfo.ojama[0];
				//			pinfo.ojama[0] = 0;
				//		}
				//	}
				//}
				if (depth + 1== beamdropdepth && beamfirstdropnum > 0) {
					pinfo.ojama[0] += beamfirstdropnum;
					pinfo.checkojama();
				}
				pinfo.ojamadrop(depth + 1, true);

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

				if (!isskill) {
					pinfo.calcbeamhyouka();
				}
				else {
					if (pinfo.chain > 0) {
						// 自分のスキルを 8 増やす
						pinfo.skill[0] += 8;
						// 100を超えたらここで100に補正する（相手の連鎖によるスキルの減少より前に計算する必要がある点に注意！）
						if (pinfo.skill[0] > 100) {
							pinfo.skill[0] = 100;
						}
					}
					pinfo.calcbeamhyouka_skill();
				}
				//if (depth == 2 && pinfoorig.prev->px == 5 && pinfoorig.prev->pr == 2 && pinfoorig.px == 6 && pinfoorig.pr == 1) {
				//	cerr << "x " << x << "," << r << "," << pinfo.hyouka.total << "," << pinfo.hyouka.chain << "," << pinfo.hyouka.normalblocknum << endl;
				//	pinfo.dump_field();
				//}
				aicount += 36;
				pinfo.px = x;
				pinfo.pr = r;
				{
					std::lock_guard<std::mutex> lock(mtx);
					beam_pdata[depth + 1][pinfo.hyouka.chain].push_back(pinfo);
				}
			}
		}
		count++;
	}
}

// 一手目に対する最大連鎖数
//double bestbeamfirsthyouka[9][4];
//double bestbeamfirsthyouka_total[9][4];

int beamsearch_thread(bool returndepth = false, int beamdropdepth = 0, int beamfirstdropnum = 0, bool isskill = false) {
	PlayerInfo pinfo;
	ginfo.istimeout = false;
	ginfo.firstmaxchaindepth = -1;
	// 置換表の初期化
	if (parameter.usett) {
		tt.clear();
	}
	for (int i = 0; i <= MAX_CHAIN; i++) {
		ginfo.eachchaindepth[i] = 1000;
	}
	//for (int x = 0; x <= 8; x++) {
	//	for (int r = 0; r < 4; r++) {
	//		bestbeamfirsthyouka[x][r] = 0;
	//	}
	//}
	int maxchain = 0;
	int maxchaindepth = 0;
	int beamchainmaxdepth = 0;
	for (int i = 0; i < MAX_CHAIN; i++) {
		beam_pdata[0][i].clear();
	}
	history[0].ojamadrop(0, true);
	// 初手の連鎖数は気にしない
	beam_pdata[0][0].push_back(history[0]);

	bool changehash = true;
	beamDepth = parameter.beamsearchDepth;
	beamWidth = parameter.beamsearchWidth;
	beamChainWidth = parameter.beamsearchChainWidth;
	int depth;
	bool foundbeamchainmax = false;
	if (parameter.debugmsg) {
		cerr << endl;
	}
	for (depth = 0; depth < beamDepth; depth++) {
		if (ginfo.t.time() >= parameter.firsttimelimit) {
			ginfo.istimeout = true;
			break;
		}
		size_t count = 0;
		for (int i = 0; i < MAX_CHAIN; i++) {
			beam_pdata[depth + 1][i].clear();
		}
		std::vector<std::thread> threads;
		size_t leftnum = beamWidth;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			size_t size = beam_pdata[depth][chain].size();
			if (size == 0) {
				continue;
			}
			if (ginfo.t.time() >= parameter.firsttimelimit || leftnum <= 0) {
				if (leftnum > 0) {
					ginfo.istimeout = true;
				}
				break;
			}
			size_t maxnum = size;
			if (maxnum > beamChainWidth) {
				maxnum = beamChainWidth;
			}
			if (maxnum > leftnum) {
				maxnum = leftnum;
			}
			leftnum -= maxnum;
//			maxnum = beamChainWidth;
			int chain_count = 0;
			// 評価値の順にソートする
			// partial_sort は速くなかったので使わない
			//if (size > beamChainWidth) {
			//	std::partial_sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].begin() + beamChainWidth, beam_pdata[depth][chain].end());
			//}
			// ソートしてから呼ぶ必要がある
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());
			threads.emplace_back(beamsearch_pdata, std::ref(beam_pdata[depth][chain]), depth, size, maxnum, beamdropdepth, beamfirstdropnum, isskill, false);
		}

		for (auto& t : threads) {
			t.join();
		}

		bool isfirst = true;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			if (beam_pdata[depth][chain].size() > 0) {
				if (((isfirst && parameter.turndebugmsg) || parameter.debugmsg) && !parameter.checkbeamchain && ginfo.turn == 0) {
					auto pinfoorig = beam_pdata[depth][chain].begin();
					cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " t " << setw(2) << threads.size() << " cc " << setw(3) << beam_pdata[depth][chain].size() << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig->hyouka.total << " " << defaultfloat << setw(4) << pinfoorig->hyouka.chain << " " << setw(4) << pinfoorig->hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms " << endl;
					isfirst = false;
				}
			}
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

		if (!foundbeamchainmax && parameter.beamChainmax > 0 && maxchain >= parameter.beamChainmax) {
			ginfo.firstmaxchaindepth = depth + 1;
			beamchainmaxdepth = depth + 1;
			ginfo.beamchainmaxdepth = beamchainmaxdepth;
			if (parameter.stopbeamChainmax) {
				depth++;
				break;
			}
			foundbeamchainmax = true;
			// maxchain 未満の次のbeamのデータを削除する
			for (int i = 0; i < maxchain - parameter.beamChainmaxminus; i++) {
				beam_pdata[depth + 1][i].clear();
			}
			parameter.beamhyoukamul = parameter.nextbeamhyoukamul;
		}
		if (parameter.beamChainmax2 > 0 && maxchain >= parameter.beamChainmax2) {
			depth++;
			break;
		}
		if (count == 0) {
			depth--;
			break;
		}
	}
	if (depth < 0) {
		depth = 0;
	}
	beamDepth = depth;
	double maxhyouka = -INF;
	for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
		if (beam_pdata[depth][chain].size() > 0) {
			std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());
			if (beam_pdata[depth][chain].begin()->hyouka.total > maxhyouka) {
				bestbeamhistory[depth] = *beam_pdata[depth][chain].begin();
				maxhyouka = bestbeamhistory[depth].hyouka.total;
			}
			if (chain > maxchain) {
				maxchain = chain;
				maxchaindepth = depth;
			}
			if (ginfo.eachchaindepth[chain] > depth) {
				ginfo.eachchaindepth[chain] = depth;
			}			// 
			if (ginfo.t.time() >= parameter.beamtimelimit) {
				ginfo.istimeout = true;
				break;
			}
			if (parameter.beamChainmax2 > 0) {
				break;
			}
		}
		//for (auto& pdata : beam_pdata[depth][chain]) {
		//	if (pdata.hyouka.total > bestbeamfirsthyouka[pdata.fx][pdata.fr]) {
		//		bestbeamfirsthyouka[pdata.fx][pdata.fr] = pdata.hyouka.total;
		//	}
		//}
	}

	if (!foundbeamchainmax && parameter.beamChainmax > 0 && maxchain >= parameter.beamChainmax) {
		ginfo.firstmaxchaindepth = depth;
		beamchainmaxdepth = depth;
		ginfo.beamchainmaxdepth = depth;
	}
	ginfo.maxchaindepth = maxchaindepth;
	//for (int x = 0; x <= 8; x++) {
	//	for (int r = 0; r < 4; r++) {
	//		bestbeamfirsthyouka_total[x][r] += bestbeamfirsthyouka[x][r];
	//		cerr << "x " << x << " r " << r << " " << bestbeamfirsthyouka[x][r] << "," << bestbeamfirsthyouka_total[x][r] << endl;
	//	}
	//}

	if (!parameter.checkbeamchain) {
		if (parameter.turndebugmsg) {
			if (ginfo.turn == 0) {
				cerr << "depth " << setw(2) << beamDepth << " c " << setw(3) << maxchain << " hyouka " << fixed << setprecision(2) << setw(7) << bestbeamhistory[depth].hyouka.total << " " << defaultfloat << setw(4) << bestbeamhistory[depth].hyouka.chain << " " << setw(4) << bestbeamhistory[depth].hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms" << endl;
			}
			else {
				cerr << "bd " << beamDepth << " mch " << maxchain << "/" << maxchaindepth << " t " << setw(5) << ginfo.t.time() << " ";
//				cerr << "bd " << setw(2) << beamDepth << " mch " << setw(3) << maxchain << " t " << setw(5) << ginfo.t.time() << " ";
			}
		}
	}
	else {
		// checkbeamchain の場合はdepthを返す（todo:beamDepth に depth を代入しているのだから、これはいらない？）
		cout << depth << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << maxchain << "\t" << maxchaindepth << "\t" << beamchainmaxdepth << "\t";
		cerr << depth << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << maxchain << "\t" << maxchaindepth << "\t" << beamchainmaxdepth << "\t";
		bestbeamhistory[depth].hyouka.fiveedge = beamchainmaxdepth;
		return depth;
	}

	PlayerInfo *ptr = bestbeamhistory[depth].prev;
	while (ptr) {
		depth--;
		bestbeamhistory[depth] = *ptr;
		ptr = ptr->prev;
	}
	if (parameter.debugmsg) {
		dump_fields(bestbeamhistory, beamDepth, true);
	}
	//for (int i = 0; i < MAX_BEAM_DEPTH; i++) {
	//	for (int j = 0; j <= MAX_CHAIN; j++) {
	//		beam_pdata[i][j].clear();
	//		beam_pdata[i][j].shrink_to_fit();
	//	}
	//}
	return maxchain;
}

int beamsearch_first(bool returndepth = false) {
	PlayerInfo pinfo;
	ginfo.istimeout = false;
	ginfo.firstmaxchaindepth = -1;
	// 置換表の初期化
	if (parameter.usett) {
		tt.clear();
	}
	for (int i = 0; i <= MAX_CHAIN; i++) {
		ginfo.eachchaindepth[i] = 1000;
	}

	int maxchain = 0;
	int maxchaindepth = 0;
	int beamchainmaxdepth = 0;
	for (int i = 0; i < MAX_CHAIN; i++) {
		beam_pdata[0][i].clear();
	}
	history[0].ojamadrop(0, true);
	// 初手の連鎖数は気にしない
	beam_pdata[0][0].push_back(history[0]);

	bool changehash = true;
	beamDepth = 20;
	beamWidth = parameter.beamsearchWidth;
	beamChainWidth = parameter.beamsearchChainWidth;
	int beamojamasearchturn = 1;
	int depth;
	bool foundbeamchainmax = false;
	if (parameter.debugmsg) {
		cerr << endl;
	}
	for (depth = 0; depth < beamDepth; depth++) {
		if (ginfo.t.time() >= parameter.firsttimelimit) {
			ginfo.istimeout = true;
			break;
		}
		size_t count = 0;
		for (int i = 0; i < MAX_CHAIN; i++) {
			beam_pdata[depth + 1][i].clear();
		}
		std::vector<std::thread> threads;
		size_t leftnum = beamWidth;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			size_t size = beam_pdata[depth][chain].size();
			if (size == 0) {
				continue;
			}
			if (ginfo.t.time() >= parameter.firsttimelimit || leftnum <= 0) {
				if (leftnum > 0) {
					ginfo.istimeout = true;
				}
				break;
			}
			size_t maxnum = size;
			if (maxnum > beamChainWidth) {
				maxnum = beamChainWidth;
			}
			if (maxnum > leftnum) {
				maxnum = leftnum;
			}
			leftnum -= maxnum;
			int chain_count = 0;
			threads.emplace_back(beamsearch_pdata, std::ref(beam_pdata[depth][chain]), depth, size, maxnum, 0, 0, false, false);
		}

		for (auto& t : threads) {
			t.join();
		}

		bool isfirst = true;
		for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
			if (beam_pdata[depth][chain].size() > 0) {
				if (((isfirst && parameter.turndebugmsg) || parameter.debugmsg) && !parameter.checkbeamchain && ginfo.turn == 0) {
					auto pinfoorig = beam_pdata[depth][chain].begin();
					cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " t " << setw(2) << threads.size() << " cc " << setw(3) << beam_pdata[depth][chain].size() << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig->hyouka.total << " " << defaultfloat << setw(4) << pinfoorig->hyouka.chain << " " << setw(4) << pinfoorig->hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms " << endl;
					isfirst = false;
				}
			}
			if (beam_pdata[depth + 1][chain].size() > 0) {
				// ここでソートを済ませておく（この後、pinfoorig.beampinfo を設定してからソートするとずれてうまくいかなくなるので）
				std::sort(beam_pdata[depth + 1][chain].begin(), beam_pdata[depth + 1][chain].end());
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

		// beamChainmax 以上の連鎖を達成した最初の深さの場合、ここで探索を打ち切る
		if (foundbeamchainmax == false && maxchain >= parameter.beamChainmax) {
			// 達成した深さを記録する
			ginfo.firstmaxchaindepth = depth + 1;
			beamchainmaxdepth = depth + 1;
			foundbeamchainmax = true;
			beamDepth = beamchainmaxdepth + beamojamasearchturn - 1;
			for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
				if (beam_pdata[beamchainmaxdepth][chain].size() > 0) {
					for (auto& pinfoorig : beam_pdata[beamchainmaxdepth][chain]) {
						pinfoorig.beampinfo = &pinfoorig;
					}
				}
			}
		}
		if (count == 0) {
			depth--;
			break;
		}
	}
	if (depth < 0) {
		depth = 0;
	}
	beamDepth = depth;
	int beamsearchedDepth = depth;

	// 時間切れになった時のための通常の結果を計算しておく
	double maxhyouka = -INF;
	for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
		if (beam_pdata[depth][chain].size() > 0) {
			if (beam_pdata[depth][chain].begin()->hyouka.total > maxhyouka) {
				bestbeamhistory[depth] = *beam_pdata[depth][chain].begin();
				maxhyouka = bestbeamhistory[depth].hyouka.total;
			}
			if (chain > maxchain) {
				maxchain = chain;
				maxchaindepth = depth;
			}
			if (ginfo.eachchaindepth[chain] > depth) {
				ginfo.eachchaindepth[chain] = depth;
			}
			if (ginfo.t.time() >= parameter.beamtimelimit) {
				ginfo.istimeout = true;
				break;
			}
		}
	}

	ginfo.firstbeamcheck = false;

	PlayerInfo *ptr = bestbeamhistory[depth].prev;
	while (ptr) {
		depth--;
		bestbeamhistory[depth] = *ptr;
		ptr = ptr->prev;
	}

	if (ginfo.t.time() >= parameter.beamtimelimit) {
		ginfo.istimeout = true;
		return maxchain;
	}

	parameter.beamminx = 0;
	parameter.beammaxx = 8;
	PlayerInfo *bestbeampinfo = nullptr;

	if (parameter.turndebugmsg) {
		cerr << "bcd " << beamDepth << " " << beamchainmaxdepth << " " << foundbeamchainmax << " " << ginfo.t.time() << endl;
	}

	beamWidth /= 2;
	beamChainWidth /= 2;

	int lefttime = parameter.firsttimelimit - ginfo.t.time();
	if (parameter.firstbeamreducetimelimit <= 0) {
		parameter.firstbeamreducetimelimit = 10000;
	}
	if (lefttime < parameter.firstbeamreducetimelimit) {
		beamWidth = beamWidth * lefttime / parameter.firstbeamreducetimelimit;
		beamChainWidth = beamChainWidth * lefttime / parameter.firstbeamreducetimelimit;
		if (parameter.turndebugmsg) {
			cerr << "reducebeam " << (lefttime * 100.0 / parameter.firstbeamreducetimelimit) << "% " << beamWidth << "," << beamChainWidth << endl;
		}
	}
	//	int checkdepthnum = 0;
	//	cerr << "x1 " << beamchainmaxdepth << endl;
	//	// parameter.beamChainmax 以上の連鎖が見つかっている場合
	if (foundbeamchainmax && beamsearchedDepth >= beamchainmaxdepth) {
		//cerr << "x2 " << beamchainmaxdepth << "," << beamsearchedDepth << endl;
		// beammaxchaindepth + x ターンにおじゃまが 30 降った場合の 最大連鎖数(ただし、x=0の場合はお邪魔は降らない）を表す
		// bemmaxchain[x] の初期化
		for (int d = beamchainmaxdepth; d <= beamsearchedDepth; d++) {
			for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
				if (beam_pdata[d][chain].size() > 0) {
					for (auto& pinfoorig : beam_pdata[d][chain]) {
						if (d == beamchainmaxdepth) {
							memset(pinfoorig.beammaxchain, 0, PlayerInfo::BEAMSEARCH2_MAXDEPTH);
						}
						pinfoorig.canbeammanychain = false;
						if (d == beamsearchedDepth) {
							pinfoorig.ignore = false;
						}
						else {
							pinfoorig.ignore = true;
						}
					}
				}
			}
		}
		if (ginfo.t.time() >= parameter.firsttimelimit) {
			ginfo.istimeout = true;
			return maxchain;
		}
		// beammaxchaindepth + turnplus でお邪魔が 30 降ってきた場合の3ターン以内の最大連鎖数を計算する
		// ただし、turnplus = 0 の場合はお邪魔は降らないらなかった場合
		//cerr << "a1 " << ginfo.t.time() << endl;
		constexpr int depthinterval = 10;
		for (int turnplus = beamsearchedDepth - beamchainmaxdepth; turnplus >= 0; turnplus--) {
			//cerr << "tplus " << turnplus << endl;
			if (ginfo.t.time() >= parameter.firsttimelimit) {
				ginfo.istimeout = true;
				return maxchain;
			}
			int startdepth = beamchainmaxdepth + turnplus;
			for (depth = startdepth; depth <= startdepth + 10; depth++) {
				if (ginfo.t.time() >= parameter.firsttimelimit) {
					break;
//					ginfo.istimeout = true;
//					return maxchain;
				}
				// チェックしたいデータが入っている beam_pdata の深さ
				// 混じらないように turnplus 毎に + depthinterval してずらす（最初の-1の時は本来のデータを使う） 
				int currentdepthindex = depth;
				if (depth != startdepth) {
					currentdepthindex += (turnplus + 1) * depthinterval;
				}
				int nextdepthindex = depth + (turnplus + 1) * depthinterval + 1;
				//cerr << "y " << turnplus << " " << depth << " " << currentdepthindex << " " << nextdepthindex << " " << endl;

				size_t count = 0;
				// 次の beam_pdata の初期化
				for (int j = 0; j < MAX_CHAIN; j++) {
					beam_pdata[nextdepthindex][j].clear();
				}
				//cerr << "z " << endl;
				std::vector<std::thread> threads;
				size_t leftnum = beamWidth;

				for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
					size_t size = beam_pdata[currentdepthindex][chain].size();
					if (size == 0) {
						continue;
					}
					//cerr << "t " << turnplus << " d " << depth << " c " << chain << " i " << currentdepthindex << " s " << size << endl;
					//					cerr << "z1 " << chain << endl;
					// 最初の深さのみ、お邪魔を30の状態で探索する（このターンに敵が降らせてくるという仮定。このお邪魔が落ちるのはこの次のターン）
					size = 0;
					// ojamadroppinfo がずれるので、先にソートしておく必要がある
					for (auto& pinfoorig : beam_pdata[currentdepthindex][chain]) {
						if (depth == startdepth) {
							pinfoorig.ojama[0] = 45;
							pinfoorig.ojamadroppinfo = &pinfoorig;
						}
						if (!pinfoorig.ignore) {
							size++;
						}
					}
					if (size == 0) {
						continue;
					}
					size_t maxnum = size;
					if (maxnum > beamChainWidth) {
						maxnum = beamChainWidth;
					}
					if (maxnum > leftnum) {
						maxnum = leftnum;
					}
					leftnum -= maxnum;
					int chain_count = 0;
					threads.emplace_back(beamsearch_pdata, std::ref(beam_pdata[currentdepthindex][chain]), nextdepthindex - 1, size, maxnum, 0, 0, false, true);
				}

				for (auto& t : threads) {
					t.join();
				}
				if (ginfo.t.time() >= parameter.firsttimelimit) {
					break;
//					ginfo.istimeout = true;
//					return maxchain;
				}
				
				bool isfirst = true;
				for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
					if (ginfo.t.time() >= parameter.firsttimelimit) {
						break;
//						ginfo.istimeout = true;
//						return maxchain;
					}
					//cerr << "z31 " << endl;
										//					cerr << "z4 " << chain << endl;
					if (beam_pdata[currentdepthindex][chain].size() > 0) {
						if (((isfirst && parameter.turndebugmsg) || parameter.debugmsg) && !parameter.checkbeamchain) {
							auto pinfoorig = beam_pdata[currentdepthindex][chain].begin();
							cerr << "depth " << setw(2) << depth << " chain " << setw(3) << chain << " t " << setw(2) << threads.size() << " cc " << setw(3) << beam_pdata[depth][chain].size() << " hyouka " << fixed << setprecision(2) << setw(7) << pinfoorig->hyouka.total << " " << defaultfloat << setw(4) << pinfoorig->hyouka.chain << " " << setw(4) << pinfoorig->hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms " << endl;
							isfirst = false;
						}
					}
					// 次の深さのデータが存在する場合
					if (beam_pdata[nextdepthindex][chain].size() > 0) {
						// ここで先にソートしてく
						std::sort(beam_pdata[nextdepthindex][chain].begin(), beam_pdata[nextdepthindex][chain].end());
						for (auto& pinfoorig : beam_pdata[nextdepthindex][chain]) {
							// 最大連鎖数が parameter.beamChainmax 以上の場合は pinfooig.nextbeampinfo の canbeammanychain を true にする
							if (pinfoorig.maxchain >= parameter.beamChainmax) {
								//cerr << "x " << pinfoorig.ojamadroppinfo << endl;
								pinfoorig.ojamadroppinfo->canbeammanychain = true;
								if (pinfoorig.ojamadroppinfo->beammaxchain[turnplus] < pinfoorig.maxchain) {
									//cerr << "z " << (int)pinfoorig.maxchain<< endl;
									pinfoorig.ojamadroppinfo->beammaxchain[turnplus] = pinfoorig.maxchain;
									pinfoorig.ojamadroppinfo->beammaxpinfo[turnplus] = &pinfoorig;
								}
							}
							//								if (pinfoorig.beampinfo->beammaxchain[turnplus] < pinfoorig.maxchain) {
							//									cerr << "z3 " << (int)pinfoorig.maxchain << endl;
							//									if (pinfoorig.maxchain > 20) {
							//										pinfoorig.dump_field();
							//									}
							//									pinfoorig.beampinfo->beammaxchain[turnplus] = pinfoorig.maxchain;
							//////									cerr << "z325 " << pinfoorig.beampinfo << " " << turnplus << endl;
							//									pinfoorig.beampinfo->beammaxpinfo[turnplus] = &pinfoorig;
							//////									cerr << "z326 " << pinfoorig.beampinfo << " " << turnplus << endl;
							//								}
							//							}
														//cerr << "z322 " << endl;
						}
					}
				}
			}
	//		cerr << "qw" << endl;
			// 最後の探索でない場合は、次の index の ignore を設定する
			if (turnplus != 0) {
				for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
					// 今回の探索の最初の行動を調べる
					if (beam_pdata[startdepth][chain].size() > 0) {
						// 大連鎖可能な行動のうち、一つ前の深さの ignore を false にする
						for (auto& pinfoorig : beam_pdata[startdepth][chain]) {
							if (pinfoorig.canbeammanychain) {
								pinfoorig.prev->ignore = false;
							}
						}
					}
				}
			}
//			cerr << "qw2" << endl;
		}

		//cerr << "a2 " << ginfo.t.time() << endl;
		// 
		int canbeamtotal = 0;
		for (depth = beamchainmaxdepth; depth <= beamsearchedDepth; depth++) {
			int turnplus = depth - beamchainmaxdepth;
			int c = 0;
			for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
				for (auto& pinfoorig : beam_pdata[depth][chain]) {
					if (pinfoorig.canbeammanychain && (turnplus == 0 || pinfoorig.prev->canbeammanychain)) {
						c++;
						canbeamtotal++;
						//if (pinfoorig.beampinfo->beammaxchain[turnplus] < pinfoorig.beammaxchain[turnplus]) {
						//	//cerr << "tp " << turnplus << " " << (int)pinfoorig.beammaxchain[turnplus] << endl;
						//	pinfoorig.beampinfo->beammaxchain[turnplus] = pinfoorig.beammaxchain[turnplus];
						//	pinfoorig.beampinfo->beammaxpinfo[turnplus] = pinfoorig.beammaxpinfo[turnplus];
						//}
					}
					else {
						pinfoorig.canbeammanychain = false;
					}
				}
			}
			if (parameter.turndebugmsg) {
				cerr << "d " << depth << " num " << c << endl;
			}
		}
		if (canbeamtotal == 0) {
			return maxchain;
		}

		//cerr << "a3 " << ginfo.t.time() << endl;
		//		cerr << "x22" << endl;
		// beamchainmaxdepth の評価値を beammaxchain の合計とする
		//for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
		//	if (beam_pdata[beamchainmaxdepth][chain].size() > 0) {
		//		for (auto& pinfoorig : beam_pdata[beamchainmaxdepth][chain]) {
		//			pinfoorig.hyouka.total = 0;
		//			for (int j = 0; j <= beamojamasearchturn; j++) {
		//				pinfoorig.hyouka.total += pinfoorig.beammaxchain[j];
		//			}
		//		}
		//	}
		//}

		double maxhyouka = -INF;
		for (depth = beamsearchedDepth; depth >= beamchainmaxdepth; depth--) {
			for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
//				cerr << "cx " << depth << " " << chain << endl;
				for (auto& pinfoorig : beam_pdata[depth][chain]) {
					if (pinfoorig.canbeammanychain) {
						double hyouka = 0;
						PlayerInfo *ptr = &pinfoorig;
						
//						cerr << "xx " << depth << " " << chain << " " << ptr << endl;
						while (ptr && ptr->turn >= beamchainmaxdepth) {
//							cerr << ptr->turn << endl;
//							cerr << ptr->turn - beamchainmaxdepth << endl;
//							cerr << ptr->beammaxchain[ptr->turn - beamchainmaxdepth];
							hyouka += ptr->beammaxchain[ptr->turn - beamchainmaxdepth] * (beamojamasearchturn - (ptr->turn - beamchainmaxdepth));
//							cerr << hyouka << endl;
							ptr = ptr->prev;
						}
						//cerr << "hyouka " << hyouka << endl;
//						pinfoorig.beampinfo->hyouka.total = hyouka;
						if (hyouka > maxhyouka) {
							//cerr << "mh " << hyouka << " " << maxhyouka << endl;
							maxhyouka = hyouka;
							bestbeampinfo = &pinfoorig;
							//cerr << bestbeampinfo << "," << bestbeampinfo->turn << endl;
							PlayerInfo *ptr = &pinfoorig;
							while (ptr && ptr->turn >= beamchainmaxdepth) {
								//cerr << ptr->beampinfo << "," << bestbeampinfo->beampinfo << "," << ptr->turn << "," << ptr << endl;
								bestbeampinfo->beampinfo->beammaxchain[ptr->turn - beamchainmaxdepth] = ptr->beammaxchain[ptr->turn - beamchainmaxdepth];
								//cerr << (int)pinfoorig.beampinfo->beammaxchain[ptr->turn - beamchainmaxdepth] << " ";
								//PlayerInfo *ptr2 = ptr->beammaxpinfo[ptr->turn - beamchainmaxdepth];
								//cerr << "ptr2 " << ptr2 << " ";
								//while (ptr2 && ptr2->turn >= beamchainmaxdepth) {
								//	cerr << ptr2->beampinfo << "," << ptr2->turn << "," << ptr2 << endl;
								//	ptr2 = ptr2->prev;
								//}
								ptr = ptr->prev;
							}
							//cerr << endl;
							bestbeampinfo->beampinfo->hyouka.total = hyouka;
							//						cerr << "xx " << depth << " " << chain << " " << ptr << endl;
							//cerr << "hq " << bestbeampinfo->beampinfo->hyouka.total << " " << bestbeampinfo << " bmc ";
							//for (int i = 0; i < PlayerInfo::BEAMSEARCH2_MAXDEPTH; i++) {
							//	cerr << static_cast<int>(bestbeampinfo->beampinfo->beammaxchain[i]) << " ";
							//}
							//cerr << endl;
						}
					}
				}
			}
			if (maxhyouka != -INF) {
				break;
			}
		}
		beamDepth = beamchainmaxdepth;
	}
	//cerr << "a4 " << ginfo.t.time() << endl;

	if (bestbeampinfo == nullptr) {
		return maxchain;
	}

	//cerr << "x3 " << bestbeampinfo << endl;
	if (parameter.turndebugmsg) {
		cerr << "hyouka " << bestbeampinfo->beampinfo->hyouka.total << " bmc ";
		for (int i = 0; i < PlayerInfo::BEAMSEARCH2_MAXDEPTH; i++) {
			cerr << static_cast<int>(bestbeampinfo->beampinfo->beammaxchain[i]) << " ";
		}
	}
	cerr << endl;
	//double maxhyouka = -INF;
	//for (int chain = MAX_CHAIN - 1; chain >= 0; chain--) {
	//	if (beam_pdata[depth][chain].size() > 0) {
	//		std::sort(beam_pdata[depth][chain].begin(), beam_pdata[depth][chain].end());
	//		cerr << "c4 " << chain << " " << beam_pdata[depth][chain].begin()->hyouka.total << " " << maxhyouka << endl;
	//		if (beam_pdata[depth][chain].begin()->hyouka.total > maxhyouka) {
	//			bestbeamhistory[depth] = *beam_pdata[depth][chain].begin();
	//			maxhyouka = bestbeamhistory[depth].hyouka.total;
	//		}
	//		if (chain > maxchain) {
	//			maxchain = chain;
	//			maxchaindepth = depth;
	//		}
	//		if (ginfo.eachchaindepth[chain] > depth) {
	//			ginfo.eachchaindepth[chain] = depth;
	//		}			// 
	//	}
	//	//for (auto& pdata : beam_pdata[depth][chain]) {
	//	//	if (pdata.hyouka.total > bestbeamfirsthyouka[pdata.fx][pdata.fr]) {
	//	//		bestbeamfirsthyouka[pdata.fx][pdata.fr] = pdata.hyouka.total;
	//	//	}
	//	//}
	//}
	if (!foundbeamchainmax && parameter.beamChainmax > 0 && maxchain >= parameter.beamChainmax) {
		ginfo.firstmaxchaindepth = beamDepth;
		beamchainmaxdepth = beamDepth;
	}
	ginfo.maxchaindepth = maxchaindepth;
	//cerr << "hq3 " << bestbeampinfo->beampinfo->hyouka.total << " " << bestbeampinfo << " bmc ";
	//for (int i = 0; i < PlayerInfo::BEAMSEARCH2_MAXDEPTH; i++) {
	//	cerr << static_cast<int>(bestbeampinfo->beampinfo->beammaxchain[i]) << " ";
	//}
	//cerr << endl;
	if (!parameter.checkbeamchain) {
		if (parameter.turndebugmsg) {
			if (ginfo.turn == 0) {
				cerr << "depth " << setw(2) << beamDepth << " c " << setw(3) << maxchain << " hyouka " << fixed << setprecision(2) << setw(7) << bestbeamhistory[depth].hyouka.total << " " << defaultfloat << setw(4) << bestbeamhistory[depth].hyouka.chain << " " << setw(4) << bestbeamhistory[depth].hyouka.normalblocknum << " " << setw(5) << ginfo.t.time() << " ms" << endl;
			}
			else {
				cerr << "bd " << beamDepth << " mch " << maxchain << "/" << maxchaindepth << " t " << setw(5) << ginfo.t.time() << " ";
				//				cerr << "bd " << setw(2) << beamDepth << " mch " << setw(3) << maxchain << " t " << setw(5) << ginfo.t.time() << " ";
			}
		}
	}
	else {
		// checkbeamchain の場合はdepthを返す（todo:beamDepth に depth を代入しているのだから、これはいらない？）
		cout << depth << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << maxchain << "\t" << maxchaindepth << "\t" << beamchainmaxdepth << "\t";
		cerr << depth << "\t" << bestbeamhistory[depth].hyouka.chain << "\t" << maxchain << "\t" << maxchaindepth << "\t" << beamchainmaxdepth << "\t";
		bestbeamhistory[depth].hyouka.fiveedge = beamchainmaxdepth;
		return depth;
	}
	//cerr << "a5 " << ginfo.t.time() << endl;

	//PlayerInfo& bestpinfo = bestbeamhistory[depth];
	//PlayerInfo *ptr = bestpinfo.prev;
	//while (ptr) {
	//	depth--;
	//	bestbeamhistory[depth] = *ptr;
	//	ptr = ptr->prev;
	//}
	//if (parameter.debugmsg) {
	//	//dump_fields(bestbeamhistory, beamDepth, true);
	//	//cerr << "h " << bestbeampinfo->beampinfo->hyouka.total << " " << bestbeampinfo << " bmc ";
	//	for (int i = 0; i < PlayerInfo::BEAMSEARCH2_MAXDEPTH; i++) {
	//		cerr << static_cast<int>(bestbeampinfo->beampinfo->beammaxchain[i]) << " ";
	//	}
	//	cerr << endl;
	//}

	PlayerInfo *bptr = bestbeampinfo;
	for (int i = beamojamasearchturn - 1; i >= 0; i--) {
		if (bptr) {
			//cerr << "cd " << i << " " << bptr->turn << " " << static_cast<int>(bptr->beammaxchain[i]) << bptr << endl;
			//cerr << bptr->beammaxpinfo[i] << endl;
			if (!bptr->beammaxpinfo[i]) {
				continue;
			}
			//bestpinfo.dump_field();
			//bptr->dump_field();
			PlayerInfo *ptr = bptr->beammaxpinfo[i];
			while (ptr) {
				//			cerr << "a" << endl;
				//cerr << "t " << ptr->turn << " " << ptr << endl;
//				ptr->dump_field();
				//cerr << ptr->prev << endl;
				bestbeamhistory[ptr->turn] = *ptr;
				//cerr << "b" << endl;
				ptr = ptr->prev;
				//cerr << "c" << endl;
			}
			//cerr << "e" << endl;
			if (parameter.debugmsg) {
				dump_fields(bestbeamhistory, bptr->beammaxpinfo[i]->turn, true);
			}
			else if (parameter.turndebugmsg) {
				cerr << "i " << i << " " << beamchainmaxdepth << endl;
				for (int j = 0; j <= bptr->beammaxpinfo[i]->turn; j++) {
					cerr << j << " " << static_cast<int>(bestbeamhistory[j].px) << "," << static_cast<int>(bestbeamhistory[j].pr) << " " << bestbeamhistory[j].hyouka.chain << endl;
				}
			}
			//cerr << "f" << endl;
			bptr = bptr->prev;
		}
	}
	//cerr << "a6 " << ginfo.t.time() << endl;

	//for (int i = 0; i < MAX_BEAM_DEPTH; i++) {
	//	for (int j = 0; j <= MAX_CHAIN; j++) {
	//		beam_pdata[i][j].clear();
	//		beam_pdata[i][j].shrink_to_fit();
	//	}
	//}
	return maxchain;
}

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
		parameter.beamhyoukamul = parameter.firstbeamhyoukamul;
		int depth = beamsearch_thread(true);
		chaintotal += bestbeamhistory[depth].hyouka.chain;

		chaindepthtotal += depth;
		maxchaindepthtotal += bestbeamhistory[depth].hyouka.fiveedge;
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

void calcojamadroppattern(int depth, int myojamanum, int myojamadropbit, int ojamasendnum, int ojamasenddepth, int eneojamanum, int eneojamadropbit, map<int, BeamDropData>& beamdropdata, BeamDropData bdata, int maxdepth) {
	if (myojamanum >= 10) {
		myojamanum -= 10;
		if (myojamanum < 0) {
			myojamanum = 0;
		}
		myojamadropbit |= (1 << depth);
	}
	else if (eneojamanum >= 10) {
		eneojamanum -= 10;
		if (eneojamanum < 0) {
			eneojamanum = 0;
		}
		eneojamadropbit |= (1 << depth);
	}
	if (depth == maxdepth) {
		beamdropdata.emplace(myojamadropbit, bdata);
	}
	else {

		// 敵がこれまで何も落とさなかった場合のみ、次も落とさない場合のパターンを考える
		if (ojamasendnum == 0) {
			calcojamadroppattern(depth + 1, myojamanum, myojamadropbit, ojamasendnum, ojamasenddepth, eneojamanum, eneojamadropbit, beamdropdata, bdata, maxdepth);
		}
		// そうでなければ全力で落とす
		int dropnum = enemystatusdata.data[4][eneojamadropbit][depth + 1].maxojama - ojamasendnum;
		bdata.eneojamadropnum[depth + 1] = dropnum;
		myojamanum += dropnum;
		ojamasendnum = enemystatusdata.data[4][eneojamadropbit][depth + 1].maxojama;
		if (myojamanum > 0 && eneojamanum > 0) {
			if (myojamanum > eneojamanum) {
				myojamanum -= eneojamanum;
				eneojamanum = 0;
			}
			else {
				eneojamanum -= myojamanum;
				myojamanum = 0;
			}
		}
		calcojamadroppattern(depth + 1, myojamanum, myojamadropbit, ojamasendnum, ojamasenddepth != 0 ? ojamasenddepth : depth + 1, eneojamanum, eneojamadropbit, beamdropdata, bdata, maxdepth);
	}
}

// 各ターンの各種データを記録する変数
TurnData turndata;

//int x1;
//void test() {
//	x1++;
//	cerr << "a" << x1 << endl;
//	cerr.flush();
//}

// メイン関数
int main(int argc, char *argv[]) {
	//vector<std::thread> ths;
	//for (int x = 0; x < 100; x++) {
	//	ths.emplace_back(test);
	//}
	//for (auto& t : ths) {
	//	t.join();
	//}
	//cerr << x1 << endl;
	//return 0;
	//cerr << sizeof(PlayerInfo) << "," << sizeof(BitBoard) << "," << sizeof(HyoukaData);
	//return 0;

//	std::size_t num_threads = std::thread::hardware_concurrency();
//	cerr << num_threads << endl;
//	return 0;

	int dtime = 0;

	ginfo.t.restart();
	bool& skillmode = ginfo.skillmode;
	skillmode = parameter.firstskillmode;
	// 前のターンのスコア
	int prevscore[2] = { 0, 0 };
	int assumed_droppedojamanum[2] = { 0, 0 };
	int assumed_ojamanum[2] = { 0, 0 };
	// 終了までの時間を記録するタイマー
	Timer totaltime;
	// 汎用のタイマー
	Timer t;
	// ゲーム番号
	int gamenum = 0;
	// パラメータを解釈する
	parameter.parseparam(argc, argv);
	// 初期化処理を呼ぶ（かかった時間を記録する）
	turndata.inittime = init();
	// AIの名前を出力する
	cout << "ysai" << endl;
	cout.flush();

	if (parameter.usett) {
		beamtt.clear();
	}
	
	if (parameter.checkbeamchain) {
		checkbeamchain();
		return 0;
	}
	if (parameter.checkgame > 0) {
		while (parameter.checkgame != gamenum) {
			ginfo.read_start_info();
			while (!ginfo.read_start_info());
			gamenum++;
		}
	}
	else {
		// 初期情報の読み込み
		ginfo.read_start_info();
	}
	bool& beamflag = ginfo.beamflag;
	beamflag = false;
	int firstbeamdepth = 0;
	//if (parameter.aitype == AITYPE::BEAM_RENSA || parameter.aitype == AITYPE::BEAM_SKILL) {
	//	beamflag = true;
	//}
	int beamstartturn = 0;
	int lastturn = 0;

	// ターン情報を読み込む
	// ターン情報が得られる限り繰り返す
	while (ginfo.read_turn_info()) {
		// このターンで関数 ai を呼んだ回数を記録する変数の初期化
		aicount = 0;
		// 各番号のブロックが移動したかどうかを表す配列データの初期化
		blockmovecount = 0;
		for (int i = 0; i < 10; i++) {
			blockmoved[i] = -1;
		}
		ginfo.checkonlybeamxr = false;
		// 特定のターン以外を無視する処理（デバッグ用）
		if (parameter.checkturn >= 0 && ginfo.turn != parameter.checkturn) {
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

		if (parameter.checkturn >= 0 && ginfo.turn == parameter.checkturn && parameter.showchain) {
			PlayerInfo pinfo;
			history[0] = ginfo.pinfo[0];
			history[0].x = parameter.beamx;
			history[0].r = parameter.beamr;
			action(history[0], pinfo, true);
			if (parameter.beamr2 >= 0) {
				history[0] = pinfo;
				history[0].x = parameter.beamx2;
				history[0].r = parameter.beamr2;
				action(history[0], pinfo, true);
			}
			return 0;
		}

		// ターン数と残り思考時間のデバッグ表示
		if (parameter.turndebugmsg) {
			cerr << endl << "Turn " << ginfo.turn << " " << ginfo.pinfo[0].timeleft << " ms ";
		}
		if (parameter.debugmsg) {
			cerr << endl;
		}

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
		if (parameter.debugmsg) {
			cerr << endl;
			dump_fields(ginfo.pinfo, 1);
		}

		// 1手目の最大連鎖数を計算する
		// お邪魔を降らせるのでコピーする（この後 pinfo を ginfo.pinfo[0] の代わりに使ってはいけない！）
		PlayerInfo pinfo = ginfo.pinfo[0];
		pinfo.ojamadrop(0, true);
		pinfo.calcbeamhyouka();
		PlayerInfo eneinfo = ginfo.pinfo[1];
		eneinfo.ojamadrop(0, true);
		eneinfo.calcbeamhyouka();
		int mchain = pinfo.hyouka.chain;
		int echain = eneinfo.hyouka.chain;
		if (parameter.turndebugmsg) {
			cerr << "mc " << mchain << "/" << echain << " ";
		}
		beamstartturn = ginfo.turn;

		// 状況が変化した場合に、今後の方針を検討するため、味方と敵の大連鎖までのターン数を計測する
		// 状況が変化するとは以下の場合を表す
		// １．どちらかが連鎖した
		// ２．前のターンで、どちらも連鎖しなかった場合のこのターンのお互いの「フィールド上のお邪魔の数」と「たまっているお邪魔の数/10（切り捨て)」が、
		//     実際のこのターンのそれぞれの値と異なっている
		if (ginfo.pinfo[0].timeleft >= 30000 && assumed_ojamanum[0] / 10 != ginfo.pinfo[0].ojama[0] / 10 || assumed_ojamanum[1] / 10 != ginfo.pinfo[0].ojama[1] / 10 ||
			assumed_droppedojamanum[0] != ginfo.pinfo[0].bb[10].pcount() || assumed_droppedojamanum[1] != ginfo.pinfo[1].bb[10].pcount() ||
			prevscore[0] + 10 <= ginfo.pinfo[0].score || prevscore[1] + 10 <= ginfo.pinfo[1].score) {
			if (parameter.turndebugmsg) {
				cerr << "sc! " << ginfo.pinfo[0].ojama[1] << " ";
			}
			skillmode = false;
			//int mymaxchain, enemaxchain;
			//int mymaxchaindepth, enemaxchaindepth;
			//parameter.beamminx = 0;
			//parameter.beammaxx = 8;
			//parameter.beamsearchDepth = 10;
			//parameter.beamsearchWidth = 2000;
			//parameter.beamsearchChainWidth = 500;
			//parameter.beamChainmax = 10;
			//parameter.stopbeamChainmax = true;
			//parameter.beamChainmax2 = -1;
			//parameter.firsttimelimit = 2000;
			//parameter.beamhyoukamul = 0;
			//history[0] = ginfo.pinfo[0];

			//// このターンで10連鎖できない場合は、最短でいつできるかを調べる
			//if (mchain < 10) {
			//	ginfo.t.restart();

			//	if (!parameter.usethread) {
			//		mymaxchain = beamsearch(false, 0, 0);

			//	}
			//	else {
			//		mymaxchain = beamsearch_thread(false, 0, 0);
			//	}
			//	mymaxchaindepth = ginfo.maxchaindepth;
			//}
			//else {
			//	mymaxchain = mchain;
			//	mymaxchaindepth = 0;
			//}
			//if (parameter.turndebugmsg) {
			//	cerr << "myc " << mymaxchain << "/" << mymaxchaindepth << " ";
			//}
			//// 10ターン以内に、7連鎖以下しかできないようであればスキルモードにする
			////if (mymaxchain <= 6 && enemaxchain >= 8) {
			////	skillmode = true;
			////}
			////// 1ターン以内に10連鎖以上できる場合は連鎖モードにする
			////else 
			//if (mymaxchain >= 10 && mymaxchaindepth <= 1) {
			//	skillmode = false;
			//}
			//// このターンに相手にお邪魔が20以上たまっている場合は連鎖モードにする
			//else if (ginfo.pinfo[0].ojama[1] >= 20) {
			//	skillmode = false;
			//}
			//// それ以外の場合は、10ターン以内の敵の最大連鎖数を調べる
			//else {
			//	// このターンに敵が11連鎖できない場合
			//	if (echain < 11) {
			//		// 相手の11連鎖の最短ターンの計算
			//		history[0] = ginfo.pinfo[1];
			//		parameter.beamChainmax = 11;
			//		ginfo.t.restart();
			//		if (!parameter.usethread) {
			//			enemaxchain = beamsearch(false, 0, 0);

			//		}
			//		else {
			//			enemaxchain = beamsearch_thread(false, 0, 0);
			//		}
			//		enemaxchaindepth = ginfo.maxchaindepth;
			//	}
			//	else {
			//		enemaxchain = echain;
			//		enemaxchaindepth = 0;
			//	}
			//	if (parameter.turndebugmsg) {
			//		cerr << "enc " << enemaxchain << "/" << enemaxchaindepth << " ";
			//	}
			//	// 敵が11連鎖できないか、こっちが10連鎖以上できる場合で、こっちの10連鎖が敵の11連鎖のターンと同じかその次のターン以前に完成する場合
			//	if (enemaxchain < 12 || (mymaxchain >= 9 && mymaxchaindepth - 2 <= enemaxchaindepth) || (mymaxchain >= 7 && pinfo.calcskilldeleteblocknum() < 10)) {
			//		skillmode = false;
			//	}
			//	else {
			//		skillmode = true;
			//	}
			//}
		}

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

		// 相手に20以上お邪魔を降らせられる場合
		if (parameter.checkfirstchain && mchain > echain && mchain >= 7 && pinfo.timeleft >= 30000 && pinfo.ojama[1] - pinfo.ojama[0] + chain_ojamatable[mchain] >= 10) {
			if (parameter.turndebugmsg) {
				cerr << "d+ " << pinfo.ojama[1] - pinfo.ojama[0] + chain_ojamatable[mchain] << " ";
			}
			parameter.beamminx = 0;
			parameter.beammaxx = 8;
			parameter.beamsearchDepth = 5;
			parameter.beamsearchWidth = 1600;
			parameter.beamsearchChainWidth = 400;
			//				parameter.beamChainmax = -1;
			parameter.beamChainmax = -1;// parameter.beamChainmax1;
			parameter.beamChainmax2 = -1;
			parameter.firsttimelimit = 1000;
			parameter.beamhyoukamul = 0;
			parameter.stopbeamChainmax = false;

			history[0] = ginfo.pinfo[1];
			ginfo.t.restart();
			int enechain;
			// 通常時の敵のチェイン数を計算する
			if (!parameter.usethread) {
				enechain = beamsearch(false, 0, 0);

			}
			else {
				enechain = beamsearch_thread(false, 0, 0);
			}
			if (parameter.turndebugmsg) {
				cerr << "ecn " << enechain << " ";// << parameter.beamhyoukamul << " ";
			}
			// 敵が連鎖を組んでいる場合
			if (enechain >= mchain) {
				// 敵を10ターンビームサーチして最大連鎖数を計算する
				parameter.beamsearchDepth = 10;
				history[0] = ginfo.pinfo[1];
				ginfo.t.restart();
				if (!parameter.usethread) {
					enechain = beamsearch(false, 1, chain_ojamatable[mchain]);

				}
				else {
					enechain = beamsearch_thread(false, 1, chain_ojamatable[mchain]);
				}
				if (parameter.turndebugmsg) {
					cerr << "ec " << enechain << " " << (ginfo.istimeout ? "TO " : "");// << parameter.beamhyoukamul << " ";
				}

				if (enechain < mchain && (!ginfo.istimeout || beamDepth >= 7)) {
					firstx = pinfo.hyouka.normalblocknum / 10;
					firstr = pinfo.hyouka.normalblocknum % 10;
					maxfirstchain = 255;
					firstbonus = chain_ojamatable[mchain] - chain_ojamatable[enechain];
					if (parameter.turndebugmsg) {
						cerr << "go! x " << firstx << " r " << firstr << " b " << firstbonus << " ";
					}
					//					output(pinfo.hyouka.normalblocknum / 10, pinfo.hyouka.normalblocknum % 10);
					//					turndata.set(ginfo.turn, 0, 0, aicount, blockmovecount, 0, 0);
					//					continue;
				}
				else if (enechain > mchain) {
					firstx = -100;
					maxfirstchain = 5;
					chainpenalty = chain_ojamatable[enechain];
					if (parameter.turndebugmsg) {
						cerr << "p " << pinfo.hyouka.normalblocknum / 10 << "/ " << pinfo.hyouka.normalblocknum % 10 << "/" << chainpenalty << endl;
					}
				}
				else {
					firstx = -100;
					maxfirstchain = 255;
				}
			}
			else {
				firstx = -100;
				maxfirstchain = 255;
				chainpenalty = 0;
			}
		}
		else {
			firstx = -100;
			maxfirstchain = 255;
			chainpenalty = 0;
		}

		if ((parameter.aitype == AITYPE::BEAM_RENSA) && ginfo.turn == 0) {
			beamflag = true;
			aicount = 0;
			history[0] = ginfo.pinfo[0];
			//history[0].beamdropdata.clear();
			parameter.beamChainmax = parameter.beamChainmax1;
			parameter.beamhyoukamul = parameter.firstbeamhyoukamul;
			parameter.stopbeamChainmax = parameter.stopbeamChainmax;
			Timer beamtime;
			int firstmaxchain;
			if (!parameter.usethread) {
				firstmaxchain = beamsearch();
				firstbeamdepth = beamDepth;
				if (firstmaxchain >= parameter.beamChainmax1) {
					firstbeamdepth = ginfo.eachchaindepth[parameter.beamChainmax1];
				}
			}
			else {
				if (parameter.usenewfirstbeam) {
					firstmaxchain = beamsearch_first();
					firstbeamdepth = beamDepth;
				}
				else {
					firstmaxchain = beamsearch_thread();
					firstbeamdepth = ginfo.beamchainmaxdepth; // beamDepth;
					
					if (firstmaxchain >= parameter.beamChainmax1) {
						firstbeamdepth = ginfo.eachchaindepth[parameter.beamChainmax1];
					}
				}
			}
			memcpy(firstbestbeamhistory, bestbeamhistory, sizeof(PlayerInfo) * MAX_BEAM_DEPTH);

			if (parameter.turndebugmsg) {
				cerr << "fbd " << firstbeamdepth << endl;
			}
			turndata.setbeam(beamtime.time(), aicount);
			if (parameter.debugmsg && parameter.usett) {
				tt.dump_totalcount();
			}
		}

		if (ginfo.turn >= firstbeamdepth) {
			beamflag = false;
		}

		if (beamflag) {
			ginfo.beamx = firstbestbeamhistory[ginfo.turn + 1].px;
			ginfo.beamr = firstbestbeamhistory[ginfo.turn + 1].pr;
		}

		//if (parameter.aitype == AITYPE::BEAM_RENSA && !beamflag && parameter.nextchain && ginfo.pinfo[0].timeleft >= 90000) {
		//	history[0] = ginfo.pinfo[0];
		//	parameter.beamsearchWidth = 1000;
		//	parameter.beamsearchChainWidth = 250;
		//	parameter.beamChainmax = 17;
		//	parameter.beamsearchDepth = 20;
		//	beamsearch();
		//	beamflag = true;
		//	beamstartturn = ginfo.pinfo[0].turn;
		//}

		for (int i = 0; i < MAX_DEPTH; i++) {
			ginfo.hyoukabonus[i] = 0;
			ginfo.hyoukabonusx[i] = -100;
		}


		// 8ターンまでに敵がお邪魔を降らせてくる可能性はほぼないので、8ターンより前は無条件でビームサーチの行動をとらせる
		if (beamflag && ginfo.turn < firstbeamdepth && ginfo.turn < 8) {
			if (parameter.debugmsg) {
				// 最善手の場合の「敵のお邪魔の数 - 自分お邪魔の数」、「このターンに降ってくるお邪魔の数」のデバッグ表示
				cerr << " bestojama " << static_cast<int>(besthyouka.ojama) << " ojama " << ginfo.pinfo[0].ojama[0] << " nextchain " << static_cast<int>(besthistory[1].chain) << endl;
			}
			output(ginfo.beamx, ginfo.beamr);
			if (parameter.turndebugmsg) {
				cerr << " Beam x " << static_cast<int>(ginfo.beamx) << " r " << static_cast<int>(ginfo.beamr) << " " << ginfo.t.time() << endl;
			}
			turndata.set(ginfo.turn, 0, 0, aicount, blockmovecount, 0, 0);
			continue;
		}

		// 敵の探索深さの設定。味方の探索深さ - 1 とする（これで、味方の探索のリーフノード以外で敵の行動の結果を考慮できる）
		enesearchDepth = parameter.enesearchDepth;
		mysearchDepth = parameter.searchDepth;

		// 残り思考時間に応じて、探索の深さを補正する
		// 深さ4だと約500ms、3だと約20ms、2だと約5msかかるようなので、余裕を多少もって下記のようにする
		// 大体の目安
		// 自分: 深さ4 300ms  深さ3 15ms 深さ2 0.5
		// 敵:   深さ4 2500ms 深さ3 30ms 深さ2 0.2
		if (!parameter.ignoretime) {
			if (ginfo.pinfo[0].timeleft < 100 || ginfo.pinfo[0].timeleft < 5 * (MAX_TURN - ginfo.turn)) {
				if (mysearchDepth > 2) {
					mysearchDepth = 2;
				}
				if (enesearchDepth > 2) {
					enesearchDepth = 2;
				}
				if (parameter.turndebugmsg) {
					cerr << " dep 2/2 ";
				}
			}
			else if (ginfo.pinfo[0].timeleft < 10 * (MAX_TURN - ginfo.turn)) {
				if (mysearchDepth > 3) {
					mysearchDepth = 3;
				}
				if (enesearchDepth > 2) {
					enesearchDepth = 2;
				}
				if (parameter.turndebugmsg) {
					cerr << " dep 3/2 ";
				}
			}
			else if (ginfo.pinfo[0].timeleft < 30 * (MAX_TURN - ginfo.turn)) {
				if (mysearchDepth > 3) {
					mysearchDepth = 3;
				}
				if (enesearchDepth > 3) {
					enesearchDepth = 3;
				}
				if (parameter.turndebugmsg) {
					cerr << " dep 3/3 ";
				}
			}
			else if (ginfo.pinfo[0].timeleft < 100 * (MAX_TURN - ginfo.turn)) {
				if (mysearchDepth > 4) {
					mysearchDepth = 4;
				}
				if (enesearchDepth > 3) {
					enesearchDepth = 3;
				}
				if (parameter.turndebugmsg) {
					cerr << " dep 4/3 ";
				}
			}
		}
		// 先に敵の思考を行う
		// 時間の計測開始
		aicount = 0;
		t.restart();
		// 敵の思考であることを表すフラグを立てる
		isenemy = true;
		searchDepth = enesearchDepth;
		// estatusの初期化
		enemystatusdata.clear(searchDepth);
		// 深さ0の状況に敵の初期状況をコピーする
		history[0] = ginfo.pinfo[1];
		// 置換表の初期化
		if (parameter.usett) {
			tt.clear();
		}
		// 探索開始
		if (parameter.usethread) {
			ai_thread();
		}
		else {
			ai_enemy(0, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
		}
		for (int i = 0; i <= MAX_CHAIN; i++) {
			ginfo.eneeachchaindepth[i] = 1000;
		}
		ginfo.eneeachchaindepth[echain] = 0;
		ginfo.enemaxchain = echain;
		int enedepthbit = (1 << (ginfo.pinfo[0].ojama[1] / 10)) - 1;
		// スキルを使用しない場合（estatus[0])とした場合(estatus[1])の良いほうを計算し、estatus[2]に設定する
		for (int j = 0; j < (1 << searchDepth); j++) {
			for (int k = 1; k <= searchDepth; k++) {
				// 一つ前の深さの j は 1 << (k - 1) のビット（一つ前の深さでのお邪魔の落下情報を表す）を落としたものを使う
				int prevj = j & ((1 << (k - 1)) - 1);
				// 前提として、ゲームの性質上、スキルを一度使うと10ターンの間使う事は不可能
				// 0: スキルをこれまで一度も使っていない場合
				//    この場合はその深さで降ってくるお邪魔の最大値（ojama）は、一つ前の深さにおいてもスキルを一度も使っていない場合（インデックス=0）の maxojama （それまでに相手に送ったお邪魔の合計の最大値）の差分
				enemystatusdata.data[0][j][k].ojama = enemystatusdata.data[0][j][k].maxojama - enemystatusdata.data[0][prevj][k - 1].maxojama;
				// 1: スキルを前の深さでは使っていないが、それ以前に使ったことがある場合
				//    この場合はその深さで降ってくるお邪魔の最大値（ojama）は、一つ前の深さにおける、過去にスキルを使った場合(インデックス=3）の maxojama
				enemystatusdata.data[1][j][k].ojama = enemystatusdata.data[1][j][k].maxojama - enemystatusdata.data[3][prevj][k - 1].maxojama;
				// 2: スキルを前の深さで使った(それ以前には使ったことがないはず）
				//    この場合はその深さで降ってくるお邪魔の最大値（ojama）は、一つ前の深さにおける、過去にスキルを使ったことがない場合(インデックス=0）の maxojama
				enemystatusdata.data[2][j][k].ojama = enemystatusdata.data[2][j][k].maxojama - enemystatusdata.data[0][prevj][k - 1].maxojama;
				// maxskillminus も同様
				enemystatusdata.data[0][j][k].skillminus = enemystatusdata.data[0][j][k].maxskillminus - enemystatusdata.data[0][prevj][k - 1].maxskillminus;
				enemystatusdata.data[1][j][k].skillminus = enemystatusdata.data[1][j][k].maxskillminus - enemystatusdata.data[3][prevj][k - 1].maxskillminus;
				enemystatusdata.data[2][j][k].skillminus = enemystatusdata.data[2][j][k].maxskillminus - enemystatusdata.data[0][prevj][k - 1].maxskillminus;
				// enemystatusdata.data[3] は enemystatusdata.data[1] と enemystatusdata.data[2] のうち、敵にとって良いほうの値を採用する
				enemystatusdata.data[3][j][k].ischain = enemystatusdata.data[1][j][k].ischain | enemystatusdata.data[2][j][k].ischain;
				enemystatusdata.data[3][j][k].isdead = enemystatusdata.data[1][j][k].isdead & enemystatusdata.data[2][j][k].isdead;
				enemystatusdata.data[3][j][k].maxchain = max(enemystatusdata.data[1][j][k].maxchain, enemystatusdata.data[2][j][k].maxchain);
				enemystatusdata.data[3][j][k].maxojama = max(enemystatusdata.data[1][j][k].maxojama, enemystatusdata.data[2][j][k].maxojama);
				enemystatusdata.data[3][j][k].maxskillminus = max(enemystatusdata.data[1][j][k].maxskillminus, enemystatusdata.data[2][j][k].maxskillminus);
				enemystatusdata.data[3][j][k].skilldeleteblocknum = max(enemystatusdata.data[1][j][k].skilldeleteblocknum, enemystatusdata.data[2][j][k].skilldeleteblocknum);
//				enemystatusdata.data[3][j][k].ojama = max(enemystatusdata.data[1][j][k].ojama, enemystatusdata.data[2][j][k].ojama);
//				enemystatusdata.data[3][j][k].skillminus = max(enemystatusdata.data[1][j][k].skillminus, enemystatusdata.data[2][j][k].skillminus);
				enemystatusdata.data[3][j][k].ojama = enemystatusdata.data[3][j][k].maxojama - max(enemystatusdata.data[0][prevj][k - 1].maxojama, enemystatusdata.data[3][prevj][k - 1].maxojama);
				enemystatusdata.data[3][j][k].skillminus = enemystatusdata.data[3][j][k].maxskillminus - max(enemystatusdata.data[0][prevj][k - 1].maxskillminus, enemystatusdata.data[3][prevj][k - 1].maxskillminus);
				enemystatusdata.data[3][j][k].minojamanum = min(enemystatusdata.data[1][j][k].minojamanum, enemystatusdata.data[2][j][k].minojamanum);
				enemystatusdata.data[3][j][k].maxskillojama = max(enemystatusdata.data[1][j][k].maxskillojama, enemystatusdata.data[2][j][k].maxskillojama);
				// enemystatusdata.data[4] は enemystatusdata.data[0] と enemystatusdata.data[1] と enemystatusdata.data[2] のうち、敵にとって良いほうの値を採用する
				enemystatusdata.data[4][j][k].ischain = enemystatusdata.data[0][j][k].ischain | enemystatusdata.data[1][j][k].ischain | enemystatusdata.data[2][j][k].ischain;
				enemystatusdata.data[4][j][k].isdead = enemystatusdata.data[0][j][k].isdead & enemystatusdata.data[1][j][k].isdead & enemystatusdata.data[2][j][k].isdead;
				enemystatusdata.data[4][j][k].maxchain = max(enemystatusdata.data[0][j][k].maxchain, enemystatusdata.data[3][j][k].maxchain);
				enemystatusdata.data[4][j][k].maxojama = max(enemystatusdata.data[0][j][k].maxojama, enemystatusdata.data[3][j][k].maxojama);
				enemystatusdata.data[4][j][k].maxskillminus = max(enemystatusdata.data[0][j][k].maxskillminus, enemystatusdata.data[3][j][k].maxskillminus);
				enemystatusdata.data[4][j][k].skilldeleteblocknum = max(enemystatusdata.data[0][j][k].skilldeleteblocknum, enemystatusdata.data[3][j][k].skilldeleteblocknum);
				//				enemystatusdata.data[4][j][k].ojama = max(enemystatusdata.data[0][j][k].ojama, enemystatusdata.data[2][j][k].ojama);
				//				enemystatusdata.data[4][j][k].skillminus = max(enemystatusdata.data[0][j][k].skillminus, enemystatusdata.data[2][j][k].skillminus);
				enemystatusdata.data[4][j][k].ojama = enemystatusdata.data[4][j][k].maxojama - max(enemystatusdata.data[0][prevj][k - 1].maxojama, enemystatusdata.data[3][prevj][k - 1].maxojama);
				enemystatusdata.data[4][j][k].skillminus = enemystatusdata.data[4][j][k].maxskillminus - max(enemystatusdata.data[0][prevj][k - 1].maxskillminus, enemystatusdata.data[3][prevj][k - 1].maxskillminus);
				enemystatusdata.data[4][j][k].minojamanum = min(enemystatusdata.data[0][j][k].minojamanum, enemystatusdata.data[3][j][k].minojamanum);
				enemystatusdata.data[4][j][k].maxskillojama = max(enemystatusdata.data[0][j][k].maxskillojama, enemystatusdata.data[3][j][k].maxskillojama);
				// インデックスが3と4の minblocknum は使わないので計算しない
				int ebit = enedepthbit & ((1 << searchDepth) - 1);
				if (ebit == j) {
					int mc = enemystatusdata.data[4][j][k].maxchain;
					if (ginfo.eneeachchaindepth[mc] > k - 1) {
						ginfo.eneeachchaindepth[mc] = k - 1;
					}
					if (ginfo.enemaxchain < mc) {
						ginfo.enemaxchain = mc;
					}
				}
			}
		}

		if (parameter.turndebugmsg) {
			cerr << "emax " << ginfo.enemaxchain << " ";
			for (int i = ginfo.enemaxchain; i >= 0; i--) {
				for (int j = 0; j < i; j++) {
					if (ginfo.eneeachchaindepth[i] < ginfo.eneeachchaindepth[j]) {
						ginfo.eneeachchaindepth[j] = ginfo.eneeachchaindepth[i];
					}
				}
			}
			int mind = 100;
			for (int i = ginfo.enemaxchain ; i >= 0; i--) {
				if (mind > ginfo.eneeachchaindepth[i]) {
					cerr << i << ":" << ginfo.eneeachchaindepth[i] << " ";
					mind = ginfo.eneeachchaindepth[i];
				}
			}
		}
		int etime = t.time();
		if (parameter.turndebugmsg) {
			cerr << " e " << t.time() << " d " << enesearchDepth << "/" << parameter.enesearchDepth << " ";
		}


		if (beamflag == false) {
			if (parameter.aitype == AITYPE::BEAM_RENSA && parameter.nextchain && ginfo.pinfo[0].timeleft >= 30000) {
				ginfo.t.restart();
				history[0] = ginfo.pinfo[0];
				//map<int, BeamDropData> beamdropdata_map;
				//BeamDropData bdata;
				//bdata.clear();
				//calcojamadroppattern(0, ginfo.pinfo[0].ojama[0], 0, 0, 0, ginfo.pinfo[0].ojama[1], 0, beamdropdata_map, bdata, enesearchDepth);



				Timer beamtime;
				parameter.beamminx = 0;
				parameter.beammaxx = 8;
				parameter.beamsearchDepth = parameter.nextbeamsearchDepth;
				parameter.beamsearchWidth = parameter.nextbeamsearchWidth;
				parameter.beamsearchChainWidth = parameter.nextbeamsearchChainWidth;
//				parameter.beamChainmax = parameter.beamChainmax1;
				parameter.beamChainmax2 = -1;
				parameter.stopbeamChainmax = false;
				parameter.firsttimelimit = parameter.nextfirsttimelimit;
				parameter.beamhyoukamul = parameter.nextbeamhyoukamul;
				beamstartturn = ginfo.turn;

				int beamchainnum;
				if (skillmode == false) {
					if (mchain <= 10) {
						parameter.beamChainmax = 10;
					}
					//else if (mchain < 12) {
					//	parameter.beamChainmax = 12;
					//}
					else {
						parameter.beamChainmax = -1;
					}


					if (!parameter.usethread) {
						beamchainnum = beamsearch();
					}
					else {
						beamchainnum = beamsearch_thread();
					}
					ginfo.beamx = bestbeamhistory[ginfo.turn + 1 - beamstartturn].px;
					ginfo.beamr = bestbeamhistory[ginfo.turn + 1 - beamstartturn].pr;

					for (int i = 0; i <= MAX_CHAIN; i++) {
						ginfo.myeachchaindepth[i] = 1000;
						ginfo.eneeachchaindepth[i] = 1000;
					}


					ginfo.eachchaindepth[mchain] = 0;
					ginfo.mymaxchain = max(mchain, beamchainnum);
					for (int i = ginfo.mymaxchain; i >= 0; i--) {
						for (int j = 0; j < i; j++) {
							if (ginfo.eachchaindepth[i] < ginfo.eachchaindepth[j]) {
								ginfo.eachchaindepth[j] = ginfo.eachchaindepth[i];
							}
						}
						ginfo.myeachchaindepth[i] = ginfo.eachchaindepth[i];
					}
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
					
					parameter.beamsearchWidth = 750;
					parameter.beamsearchChainWidth = 250;
					//				parameter.beamChainmax = parameter.beamChainmax1;
					parameter.beamChainmax2 = -1;
					parameter.stopbeamChainmax = false;
					parameter.firsttimelimit = 500;

					ginfo.t.restart();
					history[0] = ginfo.pinfo[1];

					if (echain <= 5) {
						parameter.beamChainmax = 10;
					}
					else {
						parameter.beamChainmax = echain + 5;
					}
					if (!parameter.usethread) {
						beamchainnum = beamsearch();
					}
					else {
						beamchainnum = beamsearch_thread();
					}

					ginfo.eachchaindepth[echain] = 0;
					ginfo.enemaxchain = max(echain, beamchainnum);
					for (int i = ginfo.enemaxchain; i >= 0; i--) {
						for (int j = 0; j < i; j++) {
							if (ginfo.eachchaindepth[i] < ginfo.eachchaindepth[j]) {
								ginfo.eachchaindepth[j] = ginfo.eachchaindepth[i];
							}
						}
						ginfo.eneeachchaindepth[i] = ginfo.eachchaindepth[i];
					}
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


					// 以下の場合にスキルモードとする
					// 敵のenessearchDepthターン以内の最大連鎖数が 12 以上の場合
					//   自分のビームサーチの最大連鎖数が9以下の場合 または、
					//   自分のビームサーチの10連鎖以上の発動ターン数が敵の12連鎖の発動ターン数より2以上多く、なおかつ9連鎖の発動ターン数が、敵の12連鎖の発動ターン数以下の場合
					//   自分の12連鎖、11連鎖が相手の12連鎖よりも3ターン以上遅い
					//   自分の10連鎖が、相手の12連鎖よりも2ターン以上遅い
					//   自分の9連鎖が、相手の12連鎖と同じかそれよりも遅い
					//cerr << ginfo.pinfo[0].ojama[1] << "," << ginfo.enemaxchain << "," << ginfo.mymaxchain << "," << ginfo.myeachchaindepth[12] << "," << ginfo.myeachchaindepth[11] << "," << ginfo.myeachchaindepth[10] << "," << ginfo.myeachchaindepth[9] << "," << ginfo.eneeachchaindepth[12] << endl;
					if (ginfo.pinfo[0].bb[10].pcount() >= 20 && ginfo.pinfo[0].ojama[1] < 20 && ginfo.enemaxchain >= 12 && !(ginfo.mymaxchain >= parameter.nextbeamsearchminChain && pinfo.calcskilldeleteblocknum() < 10) &&
						(ginfo.myeachchaindepth[12] >= ginfo.eneeachchaindepth[12] + 3 && 
						 ginfo.myeachchaindepth[11] >= ginfo.eneeachchaindepth[12] + 3 &&
						 ginfo.myeachchaindepth[10] >= ginfo.eneeachchaindepth[12] + 2 &&
						 ginfo.myeachchaindepth[9] >= ginfo.eneeachchaindepth[12] + 1 &&
						 ginfo.myeachchaindepth[8] >= ginfo.eneeachchaindepth[12])) {
						if (parameter.turndebugmsg) {
							cerr << "SKILLMODE! ";
						}
						skillmode = true;
					}
					// そうでなければ、以下の場合に連鎖モードとする
					// １．お邪魔の数が10以下の場合
					// ２．parameter.nextbeamsearchDepth ターン以内に、parameter.nexebeamsearchminChain 以上の連鎖が可能な場合
					else if (ginfo.pinfo[0].bb[10].pcount() <= 10 || ginfo.mymaxchain >= parameter.nextbeamsearchminChain) {
						//			turndata.setbeam(beamtime.time(), aicount);
						//			if (parameter.debugmsg && parameter.usett) {
						//				tt.dump_totalcount();
						//			}
						//ginfo.beamx = 7;
						//ginfo.beamr = 2;
						if (parameter.turndebugmsg) {
							cerr << " bc ";
						}

					}
					else {
						skillmode = true;
					}
					if (skillmode == false) {
						// こちらがお邪魔を落とさない場合に、敵が大連鎖を落としてきた場合の、こちらの最大連鎖数を計算する
						// 敵にお邪魔が落ちるビット
						int ebit = 0;
						int myojama = ginfo.pinfo[0].ojama[0];
						int eneojama = ginfo.pinfo[0].ojama[1];
						if (parameter.turndebugmsg) {
							cerr << endl;
						}

						for (int d = 1; d <= searchDepth; d++) {
							if (eneojama >= 10) {
								ebit = ebit + (1 << (d - 1));
								eneojama -= 10;
							}
							if (myojama >= 10) {
								myojama -= 10;
							}
							// その深さで落とされる可能性のあるお邪魔の数
							int onum = chain_ojamatable[enemystatusdata.data[4][ebit][d].maxchain];
							if (onum < enemystatusdata.data[4][ebit][d].maxskillojama) {
								onum = enemystatusdata.data[4][ebit][d].maxskillojama;
							}
							// お邪魔が降ってきた結果、自身にたまっているお邪魔の数
							int onum2 = onum + myojama - eneojama;
							if (parameter.turndebugmsg) {
								cerr << "onum " << d << " " << ebit << " " << onum << " " << onum2 << endl;
							}
							// 落とされる可能性のあるお邪魔が10以上で、降ってくる可能性のあるお邪魔が10以上の場合のみチェックする
							if (onum >= 10 && onum2 >= 10) {
								parameter.beamminx = 0;
								parameter.beammaxx = 8;
								parameter.beamsearchDepth = 7;
								parameter.beamsearchWidth = 350;
								parameter.beamsearchChainWidth = 1000;
								//				parameter.beamChainmax = -1;
								parameter.beamChainmax = -1;// parameter.beamChainmax1;
								parameter.beamChainmax2 = -1;
								parameter.firsttimelimit = 500;
								parameter.beamhyoukamul = 0;
								parameter.stopbeamChainmax = false;

								history[0] = ginfo.pinfo[0];
								ginfo.t.restart();
								ginfo.checkonlybeamxr = true;
								int mychain;
								if (!parameter.usethread) {
									mychain = beamsearch(false, d, onum);
								}
								else {
									mychain = beamsearch_thread(false, d, onum);
								}
								ginfo.checkonlybeamxr = false;
								dtime += ginfo.t.time();
								ginfo.hyoukabonusx[d] = bestbeamhistory[1].px;
								ginfo.hyoukabonusr[d] = bestbeamhistory[1].pr;
								ginfo.hyoukabonus[d] = chain_ojamatable[mychain];
								if (parameter.turndebugmsg) {
									cerr << "bonus " << mychain << " x " << ginfo.hyoukabonusx[d] << "," << ginfo.hyoukabonusr[d] << " " << ginfo.hyoukabonus[d] << endl;
								}
							}
						}

						// お互いにsearchDepthまでお邪魔を降らせなかった場合の評価補正を計算する
						// こちらが極端に不利な場合は、スキルモードにしているはず
						// ここではこちらが極端に有利な場合の補正を計算する
						// まず、敵が12連鎖以上する場合
						// こちらが12連鎖以上できるときのみ計算する
						//if (ginfo.mymaxchain >= 12) {
						//	if (ginfo.enemaxchain >= 7) {
						//		int edepth = ginfo.eneeachchaindepth[7];
						//		if (ginfo.eneeachchaindepth[10] <= ginfo.eneeachchaindepth[7] + 1) {
						//			edepth--;
						//		}
						//		if (edepth >= 0) {
						//			// 相手が8連鎖しかできない時のこちらの連鎖数を計算する
						//			int c;
						//			for (c = 0; c <= ginfo.mymaxchain; c++) {
						//				if (ginfo.myeachchaindepth[c] > edepth) {
						//					break;
						//				}
						//			}
						//			c--;
						//			if (c >= 12 && c > mchain) {
						//				ginfo.hyoukabonus[0] = chain_ojamatable[c] - chain_ojamatable[7];
						//				if (parameter.turndebugmsg) {
						//					cerr << "nbonus " << c << "/" << edepth << "/" << ginfo.hyoukabonus[0] << " ";
						//				}
						//			}
						//		}
						//	}
						//	else {
						//		ginfo.hyoukabonus[0] = chain_ojamatable[ginfo.mymaxchain] - chain_ojamatable[ginfo.enemaxchain];
						//		if (parameter.turndebugmsg) {
						//			cerr << "nbonus2 " << ginfo.mymaxchain << "/" << ginfo.enemaxchain << "/" << ginfo.hyoukabonus[0] << " ";
						//		}
						//	}
						//}
					}
				}
				// skillmode が true に変更される場合があるので、else if とはしない
				if (skillmode == true) {
					parameter.beamsearchDepth = 10;
					parameter.beamsearchWidth = 1000;
					parameter.beamsearchChainWidth = 200;
					parameter.beamChainmax = -1;
					parameter.beamChainmax2 = -1;
					parameter.firsttimelimit = 500;
					if (parameter.turndebugmsg) {
						cerr << " bs ";
					}
					if (!parameter.usethread) {
						beamsearch(false, 0, 0, true);
					}
					else {
						beamsearch_thread(false, 0, 0, true);
					}
					ginfo.beamx = bestbeamhistory[ginfo.turn + 1 - beamstartturn].px;
					ginfo.beamr = bestbeamhistory[ginfo.turn + 1 - beamstartturn].pr;
				}
			}
			else {
				ginfo.beamx = -100;
			}
		}

		if (parameter.turndebugmsg) {
			if (skillmode) {
				cerr << "smode ";
			}
			else {
				cerr << "cmode ";
			}
		}

		// 特定のターン以外を無視する処理（デバッグ用）
		if (parameter.checkturn >= 0 && ginfo.turn == parameter.checkturn && parameter.beamx >= -1) {
			ginfo.beamx = parameter.beamx;
			ginfo.beamr = parameter.beamr;
		}


		// 味方の探索開始
		aicount = 0;
		// 計測時間のリセット
		t.restart();
		// 味方の探索フラグの設定
		isenemy = false;
		// 深さ最大からはじめ、besthyouka が見つからなかった場合（100%負けになる場合）は、深さを1ずつ減らして探索しなおすことで最大限あがく
		// そうしないと、最大深さて先で生き残る手が見つからない場合は、現在の時点であきらめてしまう
		for (searchDepth = mysearchDepth ; searchDepth >= 1; searchDepth--) {
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
			if (parameter.usethread) {
				ai_thread();
			}
			else {
				ai(0, history, besthistory, bestbeamhistory_log, besthyouka, bestbeamhyouka, enemystatusdata);
			}
//			cerr << "xxxxxxxxxxxxxxxx" << searchDepth << "," << besthyouka.total << " " << besthistory[0].x << " " << besthistory[0].r << endl;
			// 最大評価値が見つかった場合はループを抜ける
			if (besthyouka.total != -INF) {
				break;
			}
		}
		if (parameter.turndebugmsg) {
			cerr << " m " << t.time() << " d " << searchDepth << "/" << mysearchDepth << "/" << parameter.searchDepth << " ";
		}
		//cerr << endl << "bh " << besthyouka.total << "," << besthistory[searchDepth].hyouka.total << "," << besthyouka.enedeaddepth << endl;

		// デバッグ表示
		if (parameter.debugmsg) {
			// 最初の敵の手番で敵の盤面にお邪魔が降ってきたかどうかを表すフラグの計算
			bool isenefirstojamadropped = false;
			if (besthistory[0].eneojamadroppeddepthbit & 1) {
				isenefirstojamadropped = true;
			}
			// 敵の各状況における行動の結果のデバッグ表示
			for (int i = 1; i <= enesearchDepth; i++) {
				cerr << "depth " << i << endl;
				for (int j = 0; j < (1 << i); j++) {
					if ((isenefirstojamadropped && (j & 1) == 0) || (!isenefirstojamadropped && (j & 1) == 1)) {
						continue;
					}
					for (int k = 0; k < ESTATUS_NUM; k++) {
						cerr << "ene " << k << " ";
						for (int l = 0; l < enesearchDepth; l++) {
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
//				if (!beamflag) {
					cerr << "beam" << endl;
					dump_fields(bestbeamhistory_log, besthyouka.enedeaddepth);
//				}
			}
			// そうでなければ探索深さまで表示
			else {
				cerr << "normal" << endl;
				dump_fields(besthistory, mysearchDepth);
//				if (!beamflag) {
					cerr << "beam" << endl;
					dump_fields(bestbeamhistory_log, mysearchDepth);
//				}

			}
			if (parameter.usett) {
				tt.dump_count();
			}
		}

		//cerr << "b2" << endl;
		//beamsearch2();


		if (parameter.debugmsg) {
			cerr << "beam flag " << beamflag << " depth " << beamDepth << " turn " << ginfo.turn << endl;
		}
		if (beamflag && ginfo.turn < beamDepth + beamstartturn) {
			if (parameter.debugmsg) {
				// 最善手の場合の「敵のお邪魔の数 - 自分お邪魔の数」、「このターンに降ってくるお邪魔の数」のデバッグ表示
				cerr << "bestojama " << static_cast<int>(besthyouka.ojama) << " ojama " << ginfo.pinfo[0].ojama[0] << " nextchain " << static_cast<int>(besthistory[1].chain) << endl;
			}
			// このターンにお邪魔が降ってくる場合は、あらかじめ計算しておいた行動が無意味になるため、beamflag を false にして通常の行動をとらせる
			if (ginfo.pinfo[0].ojama[0] >= 10) {
				if (parameter.debugmsg) {
					cerr << " Ojama dropped. Beam canceled." << endl;
				}
				else if (parameter.turndebugmsg) {
					cerr << " Ojama dropped. ";
				}
				beamflag = false;
			}
			// 以下の状況ではあらかじめ計算しておいた行動を破棄する
			// 1. 最善手において、このターンで13連鎖以上(相手に50以上お邪魔を降らせる）を行い、
			//    なおかつ相手に100以上お邪魔を降らせる（勝ちが見えているので。なお、相手の探索深さより先の手で大連鎖されて逆転される可能性はある）
			// 2. 最善手でこちらにお邪魔が50以上降る場合は、相手が最善手を取らないことに期待して、そのまま予定通りの行動を取らせる
//			else if (!(besthyouka.ojama >= parameter.beamIgnoreOjamanum && besthistory[1].chain >= parameter.beamIgnoreChainnum) || besthyouka.ojama < -parameter.beamNotIgnoreOjamanum) {
			else if ((besthyouka.total == bestbeamhyouka.total) || 
				     (!(besthyouka.total >= parameter.beamIgnoreHyoukamax && bestbeamhyouka.total < parameter.beamIgnoreHyoukamax) && bestbeamhyouka.total != -INF && !(bestbeamhyouka.total < parameter.beamIgnoreHyoukamin && besthyouka.total - bestbeamhyouka.total >= parameter.beamIgnoreHyoukadiff))) {
				// この時、予定していた行動をとった結果、死亡してしまう可能性がある。
				// そのチェックを行う必要があるが・・・
				// 最善手を取っておく
				//int bestxbak, bestrbak;
				//bestxbak = besthistory[0].x;
				//bestrbak = besthistory[0].r;

				//pinfoorig.x = bestbeamhistory[ginfo.turn + 1 - beamstartturn].px;
				//pinfoorig.r = bestbeamhistory[ginfo.turn + 1 - beamstartturn].pr;
				//// 置換表の初期化
				//if (parameter.usett) {
				//	tt.clear();
				//}
				//bool cancelflag = true;
				//if (action(pinfoorig, history[1], true)) {
				//	searchDepth = 3;
				//	ai(0);
				//	if (besthyouka.total != -INF) {
				//		cancelflag = false;
				//	}
				//}

				output(ginfo.beamx, ginfo.beamr);
				if (parameter.turndebugmsg) {
					cerr << " Beam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.total << " bh " << bestbeamhyouka.total << endl;
				}
				// 味方の思考時間を計測する
				int mtime = t.time();
				// 敵と味方の思考時間を記録する
				turndata.set(ginfo.turn, mtime, etime, aicount, blockmovecount, searchDepth, enesearchDepth);
				continue;
			}
			// それ以外の場合はあらかじめ計算しておいた行動は捨てて、通常の行動をとる
			else {
				if (parameter.turndebugmsg) {
					cerr << " Canceled Beam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.total << " bh " << bestbeamhyouka.total << endl;
				}
				// ただし、あらかじめ計算しておいた行動と、通常の行動が一致しない場合のみ beamflag を false にする
				if (bestbeamhistory[ginfo.turn + 1].px != besthistory[0].x || bestbeamhistory[ginfo.turn + 1].pr != besthistory[0].r) {
					beamflag = false;
				}
			}
		}
		else if (beamflag == false && parameter.aitype == AITYPE::BEAM_RENSA && parameter.nextchain) {
			if (bestbeamhyouka.total != -INF && !(besthyouka.total - bestbeamhyouka.total >= parameter.nextbeamIgnoreHyoukadiff)) {
				output(ginfo.beamx, ginfo.beamr);
				if (parameter.turndebugmsg) {
					cerr << " NBeam x " << ginfo.beamx << " r " << ginfo.beamr << " h " << besthyouka.ojama + besthyouka.droppedojama << "/" << besthyouka.total << " bh " << bestbeamhyouka.total << endl; // "," << bestbeamhyouka.eneskillojama << endl;
				}
				// 味方の思考時間を計測する
				int mtime = t.time();
				// 敵と味方の思考時間を記録する
				turndata.set(ginfo.turn, mtime, etime, aicount, blockmovecount, searchDepth, enesearchDepth);
				continue;
			}
			else {
				if (parameter.turndebugmsg) {
					cerr << " NBC x " << ginfo.beamx << " r " << ginfo.beamr << " bh " << bestbeamhyouka.total << " "; // "," << bestbeamhyouka.eneskillojama << " ";// << bestbeamhyouka.normalblocknum << " ";
				}
			}
		}

		// 最善手を取り出す（besthistory[0]に記録されている）
		int bestx, bestr;
		bestx = besthistory[0].x;
		bestr = besthistory[0].r;
		// 最善手、その評価値などのデバッグ表示
		output(bestx, bestr);
		// 計測時間、最善手を見つけた際の探索深さの表示
		if (parameter.turndebugmsg) {
//			cerr << "Act " << bestx << " " << bestr << " h " << setprecision(3) << fixed << besthyouka.ojama + besthyouka.droppedojama << "/" << besthyouka.total << "," << besthyouka.eneskillojama;
			cerr << "Act " << bestx << " " << bestr << " h " << besthyouka.total << " ";
		}
		if (parameter.debugmsg) {
			cerr << " bmc " << blockmovecount << " aic " << aicount << " " << errornum;
		}
		if (parameter.turndebugmsg) {
			cerr << endl;
		}

		// 味方の思考時間を計測する
		int mtime = t.time();
		// 敵と味方の思考時間を記録する
		turndata.set(ginfo.turn, mtime, etime, aicount, blockmovecount, searchDepth, enesearchDepth);
		lastturn = ginfo.turn;
	}
	// ゲームオーバー後の表示
	// 実際のゲームでは、ゲームオーバー時にプロセスが強制終了するのでここには来ないはず
	// コマンドプロンプトなどから実行した場合に表示される
	// 全所要時間をデバッグ表示
	cerr << "Total time " << totaltime.time() << " dtime " << dtime <<  endl;
	// 各ターンのデータをデバッグ表示
	turndata.dump(lastturn);
	// エラーの数を表示
	cerr << "errornum " << errornum << endl;
	if (parameter.usett) {
		tt.dump_totalcount();
	}
	cerr.flush();
    return 0;
}

