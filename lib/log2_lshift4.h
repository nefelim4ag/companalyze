#include <stdint.h>

/*
 * Precalculated log2 values for 0 - 8193 range
 * return data shifted to left for 4 bit,
 * for improve precision without float point.
 *
 * Used only in shannon entropy for heuristic
 *
 * Only first 128 elements precalculated for save memory
 */
#define LOG2_RET_SHIFT (1 << 4)

static int log2_lshift4(uint16_t x)
{
	/*
	 * Predefined array for first 128 values
	 * Python generator example
	 * import math
	 * for i in range(1, 128):
	 *     print(int(math.log2(i)*16),end=', ')
	 */
	static const uint8_t ret[128] = {
		0,    0,    16,   25,   32,   37,   41,   44,   48,   50,
		53,   55,   57,   59,   60,   62,   64,   65,   66,   67,
		69,   70,   71,   72,   73,   74,   75,   76,   76,   77,
		78,   79,   80,   80,   81,   82,   82,   83,   83,   84,
		85,   85,   86,   86,   87,   87,   88,   88,   89,   89,
		90,   90,   91,   91,   92,   92,   92,   93,   93,   94,
		94,   94,   95,   95,   96,   96,   96,   97,   97,   97,
		98,   98,   98,   99,   99,   99,   99,   100,  100,  100,
		101,  101,  101,  102,  102,  102,  102,  103,  103,  103,
		103,  104,  104,  104,  104,  105,  105,  105,  105,  106,
		106,  106,  106,  106,  107,  107,  107,  107,  108,  108,
		108,  108,  108,  109,  109,  109,  109,  109,  110,  110,
		110,  110,  110,  111,  111,  111,  111,  111
	};


	if (x < 128)
		return ret[x];

	if (x < 1024) {
		if (x < 256) {
			if (x < 134)
				return 112;
			if (x < 140)
				return 113;
			if (x < 146)
				return 114;
			if (x < 153)
				return 115;
			if (x < 159)
				return 116;
			if (x < 166)
				return 117;
			if (x < 174)
				return 118;
			if (x < 182)
				return 119;
			if (x < 190)
				return 120;
			if (x < 198)
				return 121;
			if (x < 207)
				return 122;
			if (x < 216)
				return 123;
			if (x < 225)
				return 124;
			if (x < 235)
				return 125;
			if (x < 246)
				return 126;
			return 127;
		}
		if (x < 470) {
			if (x < 268)
				return 128;
			if (x < 280)
				return 129;
			if (x < 292)
				return 130;
			if (x < 305)
				return 131;
			if (x < 318)
				return 132;
			if (x < 332)
				return 133;
			if (x < 347)
				return 134;
			if (x < 363)
				return 135;
			if (x < 379)
				return 136;
			if (x < 395)
				return 137;
			if (x < 413)
				return 138;
			if (x < 431)
				return 139;
			if (x < 450)
				return 140;
			return 141;
		}
		if (x < 981) {
			if (x < 491)
				return 142;
			if (x < 512)
				return 143;
			if (x < 535)
				return 144;
			if (x < 559)
				return 145;
			if (x < 584)
				return 146;
			if (x < 609)
				return 147;
			if (x < 636)
				return 148;
			if (x < 664)
				return 149;
			if (x < 694)
				return 150;
			if (x < 725)
				return 151;
			if (x < 757)
				return 152;
			if (x < 790)
				return 153;
			if (x < 825)
				return 154;
			if (x < 862)
				return 155;
			if (x < 900)
				return 156;
			if (x < 940)
				return 157;
			return 158;
		}
		return 159;
	}

	if (x < 8193) {
		if (x < 2048) {
			if (x < 1070)
				return 160;
			if (x < 1117)
				return 161;
			if (x < 1167)
				return 162;
			if (x < 1218)
				return 163;
			if (x < 1272)
				return 164;
			if (x < 1328)
				return 165;
			if (x < 1387)
				return 166;
			if (x < 1449)
				return 167;
			if (x < 1513)
				return 168;
			if (x < 1580)
				return 169;
			if (x < 1650)
				return 170;
			if (x < 1723)
				return 171;
			if (x < 1799)
				return 172;
			if (x < 1879)
				return 173;
			if (x < 1962)
				return 174;
			return 175;
		}
		if (x < 4096) {
			if (x < 2139)
				return 176;
			if (x < 2234)
				return 177;
			if (x < 2333)
				return 178;
			if (x < 2436)
				return 179;
			if (x < 2544)
				return 180;
			if (x < 2656)
				return 181;
			if (x < 2774)
				return 182;
			if (x < 2897)
				return 183;
			if (x < 3025)
				return 184;
			if (x < 3159)
				return 185;
			if (x < 3299)
				return 186;
			if (x < 3445)
				return 187;
			if (x < 3597)
				return 188;
			if (x < 3757)
				return 189;
			if (x < 3923)
				return 190;
			return 191;
		}
		if (x < 7845) {
			if (x < 4278)
				return 192;
			if (x < 4467)
				return 193;
			if (x < 4665)
				return 194;
			if (x < 4871)
				return 195;
			if (x < 5087)
				return 196;
			if (x < 5312)
				return 197;
			if (x < 5548)
				return 198;
			if (x < 5793)
				return 199;
			if (x < 6050)
				return 200;
			if (x < 6317)
				return 201;
			if (x < 6597)
				return 202;
			if (x < 6889)
				return 203;
			if (x < 7194)
				return 204;
			if (x < 7513)
				return 205;
			return 206;
		}
		return 207;
	}
	return 208;
}
